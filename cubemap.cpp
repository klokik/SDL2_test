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
// GLuint light_color_tex[6];
GLuint fisheye_tex;
GLuint cube_tex;

//GLuint square[4];
AEMesh mesh;

GLuint quad_buf;

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

GLint af_pos;
GLint uf_tex;


GLuint rlight_prog;
GLuint rshadow_prog;
GLuint fisheye_prog;

AEMatrix4f4 cam_mv;
AEMatrix4f4 cam_prj;

AEMatrix4f4 light_mv;
AEMatrix4f4 light_prj;

AEMatrix4f4 shadow_matrix;

float t = 0;
float fov_val = 90;

Vec3f cpos = vec3f(0,5,0);
Vec3f cang = vec3f(0,90,0);


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

	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
		die("Unable to initialize video");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	mainwindow = SDL_CreateWindow("SDL2 application",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,depth_tex_size*4,depth_tex_size*3,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	if(!mainwindow)
		die("Unable to create window!");

	checkSDLError(__LINE__);

	maincontext = SDL_GL_CreateContext(mainwindow);
	checkSDLError(__LINE__);

	SDL_GL_SetSwapInterval(1);	// turn on Vsync

	init();

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

	glClearColor(0.5f,0.5f,0.5f,1.0f);
	glEnable(GL_DEPTH_TEST);
	// glDepthFunc(GL_LEQUAL);
	// glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);	

	cam_prj = getProjMtx(fov_val,0.1,100.0f);
	light_prj = getProjMtx(90.0f,0.1,100.0f);

	// glEnable(GL_TEXTURE_2D);

	initPrograms();
	checkError();
	initFbos();
	checkError();
	initGeometry();
	checkError();
}

void deinit(void)
{
	glDeleteTextures(1,&light_depth_tex);
	glDeleteTextures(1,&fisheye_tex);
	glDeleteTextures(1,&cube_tex);

	glDeleteFramebuffers(1,&light_fbo);

	glDeleteProgram(rlight_prog);
	glDeleteProgram(rshadow_prog);

	glDeleteBuffers(1,&mesh.idvtx);
	glDeleteBuffers(1,&mesh.idtcr);
	glDeleteBuffers(1,&mesh.idfce);
	glDeleteBuffers(1,&mesh.idnrm);

	glDeleteBuffers(1,&quad_buf);
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
	setView();
	checkError();

	// GLenum copy_buf[] = {GL_BACK_LEFT};
	// glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
	// glViewport(0,0,640,640);
	// glDrawBuffers(1,copy_buf);
	// glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

	glUseProgram(rshadow_prog);

	// mv_prj = cam_prj*cam_mv;
	glUniformMatrix4fv(u_modelview,1,GL_FALSE,cam_mv.ToArray());
	glUniformMatrix4fv(u_projection,1,GL_FALSE,cam_prj.ToArray());

	glUniform3f(u_light_pos,light_mv[12],light_mv[13],light_mv[14]);
	glUniform3f(u_cam_pos,cam_mv[12],cam_mv[13],cam_mv[14]);

	glUniform1f(u_fov,fov_val);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.idfce);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idvtx);
	glVertexAttribPointer(a_pos,3,GL_FLOAT,GL_FALSE,0,0);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idnrm);
	glVertexAttribPointer(a_normal,3,GL_FLOAT,GL_FALSE,0,0);

	glEnableVertexAttribArray(a_pos);
	glEnableVertexAttribArray(a_normal);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER,light_fbo);

	AEMatrix4f4 proj[] = 
	{
		AEMatrix4f4().Translate(cpos).RotateZ(cang.X).RotateY(cang.Y).RotateY(-90).RotateX(0).Invert(),
		AEMatrix4f4().Translate(cpos).RotateZ(cang.X).RotateY(cang.Y).RotateY(90).RotateX(0).Invert(),
		AEMatrix4f4().Translate(cpos).RotateZ(cang.X).RotateY(cang.Y).RotateY(0).RotateX(-90).Invert(),
		AEMatrix4f4().Translate(cpos).RotateZ(cang.X).RotateY(cang.Y).RotateY(0).RotateX(90).Invert(),
		AEMatrix4f4().Translate(cpos).RotateZ(cang.X).RotateY(cang.Y).RotateY(0).RotateX(0).Invert(),
		AEMatrix4f4().Translate(cpos).RotateZ(cang.X).RotateY(cang.Y).RotateY(180).RotateX(0).Invert()
	};

	for(uint q=0;q<6;q++)
	{
		GLenum draw_buf[]={GL_COLOR_ATTACHMENT0+q};
		glDrawBuffers(1,draw_buf);

		glViewport(0,0,depth_tex_size,depth_tex_size);
		glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
		glUniformMatrix4fv(u_modelview,1,GL_FALSE,proj[q].ToArray());

		glDrawElements(GL_TRIANGLES,3*mesh.fcecount,GL_UNSIGNED_INT,0);
	}

	glDisableVertexAttribArray(a_pos);
	glDisableVertexAttribArray(a_normal);

	// projection
	GLenum draw_buf[]={GL_COLOR_ATTACHMENT0+6};
	glDrawBuffers(1,draw_buf);
	glUseProgram(fisheye_prog);

	checkError();
	glEnable(GL_TEXTURE_CUBE_MAP);
	glActiveTexture(GL_TEXTURE0);
	checkError();
	glBindTexture(GL_TEXTURE_CUBE_MAP,cube_tex);
	checkError();
	glUniform1i(uf_tex,0);
	checkError();

	glBindBuffer(GL_ARRAY_BUFFER,quad_buf);
	glVertexAttribPointer(af_pos,3,GL_FLOAT,GL_FALSE,0,0);
	checkError();

	glEnableVertexAttribArray(af_pos);
	glDrawArrays(GL_TRIANGLE_STRIP,0,4);
	glDisableVertexAttribArray(af_pos);
	checkError();

	// blit
	GLenum copy_buf[] = {GL_BACK_LEFT};
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
	glViewport(0,0,depth_tex_size*3,depth_tex_size*2);
	glDrawBuffers(1,copy_buf);
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_READ_FRAMEBUFFER,light_fbo);

	float dts = depth_tex_size;

	Vec2f poss[] = {
		vec2f(dts*2,dts),
		vec2f(0,dts),
		vec2f(dts,0),
		vec2f(dts,dts*2),
		vec2f(dts,dts),
		vec2f(dts*3,dts),
		vec2f(0,0)
	};

	for(uint q=0;q<7;q++)
	{
		glReadBuffer(GL_COLOR_ATTACHMENT0+q);
		glBlitFramebuffer(0,0,depth_tex_size,depth_tex_size,
			poss[q].X,poss[q].Y,
			poss[q].X+dts,poss[q].Y+dts,GL_COLOR_BUFFER_BIT,GL_NEAREST);
	}

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

		// glPatchParameteri(GL_PATCH_VERTICES,3);
	}

	if((!checkCompileStatus(vshader)) || (!checkCompileStatus(fshader)))
		throw runtime_error("invalid shader source");

	GLuint r_prog = glCreateProgram();

	glAttachShader(r_prog,vshader);
	glAttachShader(r_prog,fshader);

	if(gshader)
		glAttachShader(r_prog,gshader);

	if(tcshader)
	{
		glAttachShader(r_prog,tcshader);
		glAttachShader(r_prog,teshader);
	}

	glLinkProgram(r_prog);

	glDeleteShader(vshader);
	glDeleteShader(fshader);
	if(gshader)
		glDeleteShader(gshader);	

	if(tcshader)
	{
		glDeleteProgram(tcshader);
		glDeleteProgram(teshader);
	}

	return r_prog;
}

void initPrograms(void)
{
	//light
	// rlight_prog = loadProgram("vert_light.shd","frag_light.shd");
	// final
	rshadow_prog = loadProgram("vert_cube.shd","frag_cube.shd","","","");
	fisheye_prog = loadProgram("vert_proj.shd","frag_proj.shd","","","");

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

	af_pos = glGetAttribLocation(fisheye_prog,"af_pos");
	uf_tex = glGetUniformLocation(fisheye_prog,"uf_tex");
}

GLuint createColorTex()
{
	GLuint tex;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,depth_tex_size,depth_tex_size,0,GL_RGBA,GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

	return tex;
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

	fisheye_tex = createColorTex();
	checkError();

	glGenTextures(1,&cube_tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP,cube_tex);
	checkError();
	for(uint q=0;q<6;q++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+q,0,GL_RGBA8,depth_tex_size,depth_tex_size,0,GL_RGBA,GL_FLOAT,NULL);
		checkError();
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	checkError();

	glBindTexture(GL_TEXTURE_2D,0);
	checkError();

	// depth framebuffer
	glGenFramebuffers(1,&light_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,light_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_TEXTURE_2D,light_depth_tex,0);
	for(uint q=0;q<6;q++)
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+q,GL_TEXTURE_CUBE_MAP_POSITIVE_X+q,cube_tex,0);

	checkError();
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+6,GL_TEXTURE_2D,fisheye_tex,0);
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
	LoadObjFile(mesh,"test2.obj");

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

	Vec3f quad_data[] = {
		vec3f(-1.0,-1.0,0.5),
		vec3f(-1.0, 1.0,0.5),
		vec3f( 1.0,-1.0,0.5),
		vec3f( 1.0, 1.0,0.5)
	};

	glGenBuffers(1,&quad_buf);
	glBindBuffer(GL_ARRAY_BUFFER,quad_buf);
	glBufferData(GL_ARRAY_BUFFER,sizeof(quad_data),quad_data,GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,0);
}