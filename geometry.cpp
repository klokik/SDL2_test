#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GL/glew.h>

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
GLint u_modelview;
GLint u_projection;
GLint u_shadow_matrix;
GLint u_shadow_map;
GLint u_color_map;
GLint u_light_pos;
GLint u_cam_pos;
GLint u_fov;


GLuint rlight_prog;
GLuint rshadow_prog;

AEMatrix4f4 cam_mv;
AEMatrix4f4 cam_prj;

AEMatrix4f4 light_mv;
AEMatrix4f4 light_prj;

AEMatrix4f4 shadow_matrix;

float t = 0;
float fov_val = 60;

Vec3f cpos = vec3f(0,5,0);
Vec3f cang = vec3f(0,90,0);

GLuint vao;


void draw(SDL_Window *window);
void init(void);
void deinit(void);
void initPrograms(void);
void initFbos(void);
void initGeometry(void);
void LoadObjFile(AEMesh &mesh, const char *path);
AEMatrix4f4 getProjMtx(float fov,float z_near,float z_far);
void checkError(int line=-1);
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

	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
		die("Unable to initialize video");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	mainwindow = SDL_CreateWindow("SDL2 application",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,640,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	if(!mainwindow)
		die("Unable to create window!");

	checkSDLError(__LINE__);

	maincontext = SDL_GL_CreateContext(mainwindow);
	checkSDLError(__LINE__);

	SDL_GL_SetSwapInterval(1);	// turn on Vsync

	checkError(__LINE__);
	glewExperimental = true;
	GLenum err = glewInit();
	if(err != GLEW_OK)
		die("glewInit failed, aborting.");
	glGetError(); 	// clear error flag
	checkError(__LINE__);

	init();
	checkError(__LINE__);

	const Uint8 *state = SDL_GetKeyboardState(NULL);

	bool toggle = false;

	while(1)
	{
		SDL_PumpEvents();

		if(state[SDL_SCANCODE_Q])
			break;
		if(state[SDL_SCANCODE_J])
			t-=0.5f;
		if(state[SDL_SCANCODE_K])
			t+=0.5f;

		if(state[SDL_SCANCODE_W])	cpos.Z -= 0.1f;
		if(state[SDL_SCANCODE_S])	cpos.Z += 0.1f;
		if(state[SDL_SCANCODE_A])	cpos.X -= 0.1f;
		if(state[SDL_SCANCODE_D])	cpos.X += 0.1f;

		if(state[SDL_SCANCODE_UP])		cang.X += 0.5f;
		if(state[SDL_SCANCODE_DOWN])	cang.X -= 0.5f;
		if(state[SDL_SCANCODE_LEFT])	cang.Y += 0.5f;
		if(state[SDL_SCANCODE_RIGHT])	cang.Y -= 0.5f;

		if(state[SDL_SCANCODE_SPACE])	cpos.Y += 0.1f;
		if(state[SDL_SCANCODE_LSHIFT])	cpos.Y -= 0.1f;

		if(state[SDL_SCANCODE_O])	fov_val -= 0.5f;
		if(state[SDL_SCANCODE_P])	fov_val += 0.5f;


		if(false)
			if(toggle = !toggle)
				glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
			else
				glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		if(state[SDL_SCANCODE_R])
			glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
		if(state[SDL_SCANCODE_F])
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		draw(mainwindow);
		checkError(__LINE__);
	}

	deinit();

	SDL_GL_DeleteContext(maincontext);
	SDL_DestroyWindow(mainwindow);

	SDL_Quit();
	return 0;
}

void print_opengl_info(FILE *fp)
{
	fprintf(fp,"GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	fprintf(fp,"GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	fprintf(fp,"GL_VERSION: %s\n", glGetString(GL_VERSION));
	fprintf(fp,"GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void init(void)
{
	print_opengl_info(stdout);
	checkError(__LINE__);

	glClearColor(0.5f,0.5f,0.5f,1.0f);
	glEnable(GL_DEPTH_TEST);
	checkError(__LINE__);
	// glDepthFunc(GL_LEQUAL);
	// glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);	

	cam_prj = getProjMtx(45.0f,0.1,100.0f);
	light_prj = getProjMtx(90.0f,0.1,100.0f);

	// glEnable(GL_TEXTURE_2D);

	initPrograms();
	checkError(__LINE__);
	// initFbos();
	initGeometry();
	checkError(__LINE__);
}

void deinit(void)
{
	glDeleteTextures(1,&light_depth_tex);
	glDeleteTextures(1,&light_color_tex);

	glDeleteFramebuffers(1,&light_fbo);

	// glDeleteProgram(rlight_prog);
	glDeleteProgram(rshadow_prog);

	glDeleteBuffers(1,&mesh.idvtx);
	glDeleteBuffers(1,&mesh.idtcr);
	glDeleteBuffers(1,&mesh.idfce);
	glDeleteBuffers(1,&mesh.idnrm);
}

void setView(void)
{
	t+=0.5f;
	cam_mv = AEMatrix4f4().Translate(cpos).RotateY(cang.Y).RotateX(cang.X).Invert();//AEMatrix4f4().RotateX(30).Translate(vec3f(0,-5,-6)).RotateY(t);
	light_mv = AEMatrix4f4().RotateX(60).RotateY(180).Translate(vec3f(0,5,6)).RotateY(-t*0.7);

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
	checkError(__LINE__);
	setView();

	// GLenum copy_buf[] = {GL_BACK_LEFT};
	// glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
	glViewport(0,0,640,640);
	// glDrawBuffers(1,copy_buf);
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	checkError(__LINE__);

	glUseProgram(rshadow_prog);
	checkError(__LINE__);

	// mv_prj = cam_prj*cam_mv;
	glUniformMatrix4fv(u_modelview,1,GL_FALSE,cam_mv.ToArray());
	glUniformMatrix4fv(u_projection,1,GL_FALSE,cam_prj.ToArray());
	checkError(__LINE__);
	glUniform3f(u_light_pos,light_mv[12],light_mv[13],light_mv[14]);
	glUniform3f(u_cam_pos,cam_mv[12],cam_mv[13],cam_mv[14]);

	checkError(__LINE__);
	glUniform1f(u_fov,fov_val);
	checkError(__LINE__);

	glBindVertexArray(vao);
	checkError(__LINE__);

	glDrawElements(GL_PATCHES,3*mesh.fcecount,GL_UNSIGNED_INT,0);
	checkError(__LINE__);

	SDL_GL_SwapWindow(window);
}

bool checkCompileStatus(uint shaderid)
{
	int result;
	glGetShaderiv(shaderid,GL_COMPILE_STATUS,&result);
	if(result!=GL_TRUE)
	{
		cout<<"shader FAIL"<<endl;
		char log[2048];
		int len;
		glGetShaderInfoLog(shaderid,2048,&len,log);
		cout<<log<<endl;
		return false;
	}

	return true;
}

bool checkLinkStatus(uint programid)
{
	int result = 0;
	glGetProgramiv(programid, GL_LINK_STATUS, &result);
	if(result!=GL_TRUE)
	{
		cout<<"program FAIL"<<endl;
		char log[2048];
		int len;
		glGetProgramInfoLog(programid,2048,&len,log);
		cout<<log<<endl;
		return false;
	}

	return true;
}

uint loadProgram(string vfile,string ffile,string gfile,string tcfile,string tefile)
{
	string vert = readFile(vfile.c_str());
	string frag = readFile(ffile.c_str());

	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint gshader = 0;
	GLuint tcshader = 0;
	GLuint teshader = 0;

	const char *vbuf = vert.c_str();
	const char *fbuf = frag.c_str();

	glShaderSource(vshader,1,&vbuf,NULL);
	glShaderSource(fshader,1,&fbuf,NULL);

	glCompileShader(vshader);
	glCompileShader(fshader);

	if(gfile != "")
	{
		string geom = readFile(gfile.c_str());
		gshader = glCreateShader(GL_GEOMETRY_SHADER);

		const char *gbuf = geom.c_str();
		glShaderSource(gshader,1,&gbuf,NULL);

		glCompileShader(gshader);
		if(!checkCompileStatus(gshader))
			throw runtime_error("invalid geometry shader");		
	}

	if((tcfile != "") && (tefile != ""))
	{
		string ctrl = readFile(tcfile.c_str());
		string eval = readFile(tefile.c_str());

		tcshader = glCreateShader(GL_TESS_CONTROL_SHADER);
		teshader = glCreateShader(GL_TESS_EVALUATION_SHADER);

		const char *tcbuf = ctrl.c_str();
		const char *tebuf = eval.c_str();

		glShaderSource(tcshader,1,&tcbuf,NULL);
		glShaderSource(teshader,1,&tebuf,NULL);

		glCompileShader(tcshader);
		glCompileShader(teshader);

		if(!checkCompileStatus(tcshader) || !checkCompileStatus(teshader))
			throw runtime_error("invalid tessellatioon shader shader");	

		glPatchParameteri(GL_PATCH_VERTICES,3);
	}

	if((!checkCompileStatus(vshader)) || (!checkCompileStatus(fshader)))
		throw runtime_error("invalid shader source");

	GLuint r_prog = glCreateProgram();

	glAttachShader(r_prog,vshader);
	glAttachShader(r_prog,fshader);

	if(gfile != "") glAttachShader(r_prog,gshader);

	if(tcshader)
	{
		glAttachShader(r_prog,tcshader);
		glAttachShader(r_prog,teshader);
	}

	glLinkProgram(r_prog);
	if(!checkLinkStatus(r_prog))
		throw runtime_error("Failed to link program");

	// glDeleteShader(vshader);
	// glDeleteShader(fshader);
	// if(gfile != "")
	// 	glDeleteShader(gshader);	

	// if(tcshader)
	// {
	// 	glDeleteProgram(tcshader);
	// 	glDeleteProgram(teshader);
	// }

	return r_prog;
}

void initPrograms(void)
{
	//light
	// rlight_prog = loadProgram("vert_light.shd","frag_light.shd");
	// final
	rshadow_prog = loadProgram("vert_33.shd","frag_33.shd","","tessctrl_43.shd","tesseval_43.shd");
	checkError(__LINE__);

	// al_pos = glGetAttribLocation(rlight_prog,"al_pos");
	// ul_lmvpmat = glGetUniformLocation(rlight_prog,"ul_lmvpmat");

	a_pos = glGetAttribLocation(rshadow_prog,"a_pos");
	a_normal = glGetAttribLocation(rshadow_prog,"a_normal");
	u_mvpmat = glGetUniformLocation(rshadow_prog,"u_mvpmat");
	u_modelview = glGetUniformLocation(rshadow_prog,"u_modelview");
	u_projection = glGetUniformLocation(rshadow_prog,"u_projection");
	u_shadow_matrix = glGetUniformLocation(rshadow_prog,"u_shadow_matrix");
	u_shadow_map = glGetUniformLocation(rshadow_prog,"u_shadow_map");
	u_color_map = glGetUniformLocation(rshadow_prog,"u_color_map");
	u_light_pos = glGetUniformLocation(rshadow_prog,"u_light_pos");
	u_cam_pos = glGetUniformLocation(rshadow_prog,"u_cam_pos");
	u_fov = glGetUniformLocation(rshadow_prog,"u_fov");

	checkError(__LINE__);
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

void checkError(int line)
{
	try
	{
		GLenum err=glGetError();

		if(err != GL_NO_ERROR)
			cout << "gl error: line " << line << endl;

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
	catch(const exception &re)
	{
		cout << "opengl error exception caught: " << re.what() << endl;
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
	LoadObjFile(mesh,"test2.obj");

	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	glGenBuffers(1,&mesh.idvtx);
	glGenBuffers(1,&mesh.idfce);
	glGenBuffers(1,&mesh.idtcr);
	glGenBuffers(1,&mesh.idnrm);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idvtx);
	glBufferData(GL_ARRAY_BUFFER,sizeof(Vec3f)*mesh.vtxcount,mesh.vtx,GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.idfce);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(AETriangle)*mesh.fcecount,mesh.fce,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idtcr);
	glBufferData(GL_ARRAY_BUFFER,sizeof(AETexCoord)*mesh.tcrcount,mesh.tcr,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idnrm);
	glBufferData(GL_ARRAY_BUFFER,sizeof(Vec3f)*mesh.nrmcount,mesh.nrm,GL_STATIC_DRAW);

	glUseProgram(rshadow_prog);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idvtx);
	glVertexAttribPointer(a_pos,3,GL_FLOAT,GL_FALSE,0,0);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idnrm);
	glVertexAttribPointer(a_normal,3,GL_FLOAT,GL_FALSE,0,0);

	glEnableVertexAttribArray(a_pos);
	glEnableVertexAttribArray(a_normal);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.idfce);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER,0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
}