#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

#include "AEMatrix4f4.h"
#include "AEVectorMath.h"
#include "AEMesh.h"

using namespace std;
using namespace aengine;


int depth_tex_size = 256;
GLuint light_depth_tex;
GLuint light_fbo;
GLuint light_color_tex;

//GLuint square[4];
AEMesh mesh;

GLint al_pos;
GLint ul_lmvpmat;

GLint a_pos;
GLint a_normal;
GLint u_mvpmat;
GLint u_shadow_matrix;
GLint u_shadow_map;
GLint u_color_map;
GLint u_light_pos;
GLint u_cam_pos;


GLuint rlight_prog;
GLuint rshadow_prog;

AEMatrix4f4 cam_mv;
AEMatrix4f4 cam_prj;

AEMatrix4f4 light_mv;
AEMatrix4f4 light_prj;

AEMatrix4f4 shadow_matrix;

static float t = 0;


void draw(SDL_Window *window);
void init(void);
void deinit(void);
void initPrograms(void);
void initFbos(void);
void initGeometry(void);
void LoadObjFile(AEMesh &mesh, const char *path);
AEMatrix4f4 getProjMtx(float fov,float z_near,float z_far);
void checkError(void);
void checkFBO(GLenum fb);

void die(string msg)
{
	cout << msg << endl;
	SDL_Quit();
	exit(1);
}

void checkSDLError(int line = -1)
{
#ifndef NDEBUG
	const char *error = SDL_GetError();
	if (*error != '\0')
	{
		cout << "SDL Error: " << error << endl;
		if (line != -1)
			cout << " + line: " << line << endl;
		SDL_ClearError();
	}
#endif
}

std::string readFile(const char *filename)
{
	std::ifstream file;
	char line[256];

	std::string text;

	file.open(filename,std::ifstream::in);

	while(!file.eof())
	{
		file.getline(line,256);
		text.append(line);
		text.append("\n");
	}

	file.close();

	return text;
}

int main(int argc, char **argv)
{
	SDL_Window		*mainwindow;
	SDL_GLContext	maincontext;

	if(SDL_Init(SDL_INIT_VIDEO) < 0)
		die("Unable to initialize video");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	mainwindow = SDL_CreateWindow("SDL2 application",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,640,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	if(!mainwindow)
		die("Unable to create window!");

	checkSDLError(__LINE__);

	maincontext = SDL_GL_CreateContext(mainwindow);
	checkSDLError(__LINE__);

	SDL_GL_SetSwapInterval(1);	// turn on Vsync

	init();

	const Uint8 *state = SDL_GetKeyboardState(NULL);

	while(1)
	{
		SDL_PumpEvents();

		if(state[SDL_SCANCODE_Q])
			break;
		if(state[SDL_SCANCODE_J])
			t-=0.5f;
		if(state[SDL_SCANCODE_K])
			t+=0.5f;

		draw(mainwindow);
	}

	deinit();

	SDL_GL_DeleteContext(maincontext);
	SDL_DestroyWindow(mainwindow);

	SDL_Quit();
	return 0;
}

void init(void)
{
	glClearColor(0.0f,0.1f,0.2f,1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	// glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);	

	cam_prj = getProjMtx(45.0f,0.1,100.0f);
	light_prj = getProjMtx(90.0f,0.1,100.0f);

	// glEnable(GL_TEXTURE_2D);

	initPrograms();
	initFbos();
	initGeometry();
}

void deinit(void)
{
	glDeleteTextures(1,&light_depth_tex);
	glDeleteTextures(1,&light_color_tex);

	glDeleteFramebuffers(1,&light_fbo);

	glDeleteProgram(rlight_prog);
	glDeleteProgram(rshadow_prog);

	glDeleteBuffers(1,&mesh.idvtx);
	glDeleteBuffers(1,&mesh.idtcr);
	glDeleteBuffers(1,&mesh.idfce);
	glDeleteBuffers(1,&mesh.idnrm);
}

void setView(void)
{
	t+=0.5f;
	cam_mv = AEMatrix4f4().RotateX(30).Translate(vec3f(0,-5,-6)).RotateY(t);
	light_mv = AEMatrix4f4().RotateX(30).RotateY(180).Translate(vec3f(0,-5,6)).RotateY(-t*0.7);

	float biasf[] = {
		.5f, .0f, .0f, .0f,
		.0f, .5f, .0f, .0f,
		.0f, .0f, .5f, .0f,
		.5f, .5f, .5f, 1.f
	};

	AEMatrix4f4 scale_bias(biasf);

	shadow_matrix = scale_bias*light_prj*light_mv;
}

AEMatrix4f4 getProjMtx(float fov,float z_near,float z_far)
{
	float r_angle = fov/57.2957795131f;
	float ratio = 640/640;

	float tg = tan(r_angle/2);

	float p_mtx[16] = {
		1/tg ,0,0,0,
		0, ratio/tg, 0, 0,
		0,0,-(z_far+z_near)/(z_far-z_near),-1,
		0,0,-2*z_far*z_near/(z_far-z_near),0
	};

	return AEMatrix4f4(p_mtx);
}

void draw(SDL_Window *window)
{
	setView();

	// lightmap
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER,light_fbo);
	glViewport(0,0,depth_tex_size,depth_tex_size);
	GLenum draw_buf[]={GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1,draw_buf);
	glClearDepth(1.0f);
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

	glUseProgram(rlight_prog);

	AEMatrix4f4 mv_prj = light_prj*light_mv;
	glUniformMatrix4fv(ul_lmvpmat,1,GL_FALSE,mv_prj.ToArray());

	glEnableVertexAttribArray(al_pos);
	glBindBuffer(GL_ARRAY_BUFFER,mesh.idvtx);
	glVertexAttribPointer(al_pos,3,GL_FLOAT,GL_FALSE,0,0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.idfce);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f,4.0f);

	glDrawElements(GL_TRIANGLES,3*mesh.fcecount,GL_UNSIGNED_INT,0);

	glDisableVertexAttribArray(al_pos);
	glDisable(GL_POLYGON_OFFSET_FILL);

	// shadow
	GLenum copy_buf[] = {GL_BACK_LEFT};
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
	glViewport(0,0,640,640);
	glDrawBuffers(1,copy_buf);
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

	glUseProgram(rshadow_prog);

	mv_prj = cam_prj*cam_mv;
	glUniformMatrix4fv(u_mvpmat,1,GL_FALSE,mv_prj.ToArray());
	glUniformMatrix4fv(u_shadow_matrix,1,GL_FALSE,shadow_matrix.ToArray());

	glUniform3f(u_light_pos,light_mv[12],light_mv[13],light_mv[14]);
	glUniform3f(u_cam_pos,cam_mv[12],cam_mv[13],cam_mv[14]);

	glEnableVertexAttribArray(a_pos);
	glVertexAttribPointer(al_pos,3,GL_FLOAT,GL_FALSE,0,0);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idnrm);
	glEnableVertexAttribArray(a_normal);
	glVertexAttribPointer(a_normal,3,GL_FLOAT,GL_FALSE,0,0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,light_depth_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,light_color_tex);
	glUniform1i(u_shadow_map,0);
	glUniform1i(u_color_map,1);

	glDrawElements(GL_TRIANGLES,3*mesh.fcecount,GL_UNSIGNED_INT,0);

	glDisableVertexAttribArray(a_pos);
	glDisableVertexAttribArray(a_normal);

	// blit
	glBindFramebuffer(GL_READ_FRAMEBUFFER,light_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0,0,depth_tex_size,depth_tex_size,640-256,640-256,640,640,GL_COLOR_BUFFER_BIT,GL_NEAREST);

	SDL_GL_SwapWindow(window);
}

bool checkCompileStatus(uint shaderid)
{
	int result;
	glGetShaderiv(shaderid,GL_COMPILE_STATUS,&result);
	if(result!=GL_TRUE)
	{
		cout<<"vertex FAIL"<<endl;
		char log[2048];
		int len;
		glGetShaderInfoLog(shaderid,2048,&len,log);
		cout<<log<<endl;
		return false;
	}

	return true;
}

uint loadProgram(string vfile,string ffile)
{
	string vert = readFile(vfile.c_str());
	string frag = readFile(ffile.c_str());

	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);

	const char *vbuf = vert.c_str();
	const char *fbuf = frag.c_str();

	glShaderSource(vshader,1,&vbuf,NULL);
	glShaderSource(fshader,1,&fbuf,NULL);

	glCompileShader(vshader);
	glCompileShader(fshader);

	if((!checkCompileStatus(vshader)) || (!checkCompileStatus(fshader)))
		throw runtime_error("invalid shader source");

	GLuint r_prog = glCreateProgram();

	glAttachShader(r_prog,vshader);
	glAttachShader(r_prog,fshader);

	glLinkProgram(r_prog);

	glDeleteShader(vshader);
	glDeleteShader(fshader);

	return r_prog;
}

void initPrograms(void)
{
	//light
	rlight_prog = loadProgram("vert_light.shd","frag_light.shd");
	// final
	rshadow_prog = loadProgram("vert.shd","frag.shd");

	al_pos = glGetAttribLocation(rlight_prog,"al_pos");
	ul_lmvpmat = glGetUniformLocation(rlight_prog,"ul_lmvpmat");

	a_pos = glGetAttribLocation(rshadow_prog,"a_pos");
	a_normal = glGetAttribLocation(rshadow_prog,"a_normal");
	u_mvpmat = glGetUniformLocation(rshadow_prog,"u_mvpmat");
	u_shadow_matrix = glGetUniformLocation(rshadow_prog,"u_shadow_matrix");
	u_shadow_map = glGetUniformLocation(rshadow_prog,"u_shadow_map");
	u_color_map = glGetUniformLocation(rshadow_prog,"u_color_map");
	u_light_pos = glGetUniformLocation(rshadow_prog,"u_light_pos");
	u_cam_pos = glGetUniformLocation(rshadow_prog,"u_cam_pos");
}

void initFbos(void)
{
	// depth texture
	glGenTextures(1,&light_depth_tex);
	glBindTexture(GL_TEXTURE_2D,light_depth_tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH24_STENCIL8/*GL_DEPTH_COMPONENT32*/,depth_tex_size,depth_tex_size,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_R_TO_TEXTURE);//GL_COMPARE_REF_TO_TEXTURE);

	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

	// color texture
	glGenTextures(1,&light_color_tex);
	glBindTexture(GL_TEXTURE_2D,light_color_tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,depth_tex_size,depth_tex_size,0,GL_RGBA,GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D,0);

	// depth framebuffer
	glGenFramebuffers(1,&light_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,light_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_TEXTURE_2D,light_depth_tex,0);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,light_color_tex,0);
	// GLenum draw_buf[]={GL_COLOR_ATTACHMENT0};
	// glDrawBuffers(1,draw_buf);
	//glDrawBuffer(GL_NONE);

	checkFBO(GL_FRAMEBUFFER);
}

void checkError(void)
{
	GLenum err=glGetError();

	switch(err)
	{
	case GL_NO_ERROR:
		return;
	case GL_INVALID_ENUM:
		throw runtime_error("err: invalid enumeration;");
		break;
	case GL_INVALID_VALUE:
		throw runtime_error("err: invalid value;");
		break;
	case GL_INVALID_OPERATION:
		throw runtime_error("err: invalid operation;");
		break;
	case GL_OUT_OF_MEMORY:
		throw runtime_error("err: out of memory;");
		break;
	}
}

void checkFBO(GLenum fb)
{
	GLenum status = glCheckFramebufferStatus(fb);
	switch(status)
	{
	case GL_FRAMEBUFFER_UNDEFINED:						throw runtime_error("FRAMEBUFFER_UNDEFINED");	break;
	case GL_FRAMEBUFFER_COMPLETE:						cout<<"FRAMEBUFFER_COMPLETE"<<endl;	break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			throw runtime_error("FRAMEBUFFER_INCOMPLETE_ATTACHMENT");	break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	throw runtime_error("FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");	break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:			throw runtime_error("FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");	break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:			throw runtime_error("FRAMEBUFFER_INCOMPLETE_READ_BUFFER");	break;
	case GL_FRAMEBUFFER_UNSUPPORTED:					throw runtime_error("FRAMEBUFFER_UNSUPPORTED");	break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			throw runtime_error("FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");	break;
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:		throw runtime_error("FRAMEBUFFER_INCOMPLETE_LAYER_TARGET");	break;
	}
}

void initGeometry(void)
{
	LoadObjFile(mesh,"test.obj");

	glGenBuffers(1,&mesh.idvtx);
	glGenBuffers(1,&mesh.idfce);
	glGenBuffers(1,&mesh.idtcr);
	glGenBuffers(1,&mesh.idnrm);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idvtx);
	glBufferData(GL_ARRAY_BUFFER,sizeof(Vec3f)*mesh.vtxcount,mesh.vtx,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idfce);
	glBufferData(GL_ARRAY_BUFFER,sizeof(AETriangle)*mesh.fcecount,mesh.fce,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idtcr);
	glBufferData(GL_ARRAY_BUFFER,sizeof(AETexCoord)*mesh.tcrcount,mesh.tcr,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idnrm);
	glBufferData(GL_ARRAY_BUFFER,sizeof(Vec3f)*mesh.nrmcount,mesh.nrm,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,0);
}