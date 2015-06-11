#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <cassert>

#include "AEMatrix4f4.h"
#include "AEVectorMath.h"
#include "AEMesh.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

using namespace std;
using namespace aengine;


//GLuint square[4];
AEMesh mesh;

GLint a_pos;
GLint a_normal;
GLint a_video_coord;
GLint u_mvpmat;
GLint u_color_map;

GLuint video_frame_tex;

AEMatrix4f4 cam_mv;
AEMatrix4f4 cam_prj;

const uint16_t video_tex_size = 512;

GLuint rvideo_prog;


static float t = 0;
static float s = 30;


AVFormatContext *fmt_ctx = nullptr;
AVCodecContext *video_dec_ctx = nullptr;
AVCodecContext *audio_dec_ctx = nullptr;
AVStream *video_stream = nullptr;
AVStream  *audio_stream = nullptr;
SwsContext *img_convert_ctx;

const char *src_filename = "touhou.mp4";//nullptr;
// const char *video_dst_filename = nullptr;
// const char *audio_dst_filename = nullptr;
// FILE *video_dst_file = nullptr;
// FILE *audio_dst_file = nullptr;
uint8_t *video_dst_data[4] = {nullptr};
int      video_dst_linesize[4];
int video_dst_bufsize;
int video_stream_idx = -1;
int audio_stream_idx = -1;
AVFrame *frame = nullptr;
AVFrame *frame_rgb = nullptr;
AVPacket pkt;
int video_frame_count = 0;
int audio_frame_count = 0;
int got_frame;
int cached;


void draw(SDL_Window *window);
void init(void);
void deinit(void);
void initPrograms(void);
// void initFbos(void);
void initTextures(void);
void initGeometry(void);
void LoadObjFile(AEMesh &mesh, const char *path);
AEMatrix4f4 getProjMtx(float fov,float z_near,float z_far);
void checkError(void);
// void checkFBO(GLenum fb);

void initVideoSource(void);
int  updateVideoFrame(void);
void freeVideoSource(void);

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

		if(state[SDL_SCANCODE_H])
			s-=0.5f;
		if(state[SDL_SCANCODE_L])
			s+=0.5f;

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

	cam_prj = getProjMtx(30.0f,0.1,100.0f);

	initPrograms();
	// initFbos();
	initGeometry();

	initVideoSource();
	initTextures();
}

void deinit(void)
{
	glDeleteTextures(1,&video_frame_tex);

	glDeleteProgram(rvideo_prog);

	glDeleteBuffers(1,&mesh.idvtx);
	glDeleteBuffers(1,&mesh.idtcr);
	glDeleteBuffers(1,&mesh.idfce);
	glDeleteBuffers(1,&mesh.idnrm);

	freeVideoSource();
}

void setView(void)
{
	t+=0.5f;
	cam_mv = AEMatrix4f4().RotateX(s).Translate(vec3f(0,-3,-5)).RotateY(t);
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
	if(av_read_frame(fmt_ctx, &pkt) >= 0)
	{
		AVPacket orig_pkt = pkt;
		do {
			int ret = updateVideoFrame();
			if (ret < 0)
				break;
			pkt.data += ret;
			pkt.size -= ret;
		} while (pkt.size > 0);
		av_free_packet(&orig_pkt);
	}
	// else
	// 	return;
	// if(!got_frame)
	// 	return;

	// draw scene
	setView();

	AEMatrix4f4 mv_prj;

	glViewport(0,0,640,640);
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

	glUseProgram(rvideo_prog);

	mv_prj = cam_prj*cam_mv;
	glUniformMatrix4fv(u_mvpmat,1,GL_FALSE,mv_prj.ToArray());

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idvtx);
	glEnableVertexAttribArray(a_pos);
	glVertexAttribPointer(a_pos,3,GL_FLOAT,GL_FALSE,0,0);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idnrm);
	glEnableVertexAttribArray(a_normal);
	glVertexAttribPointer(a_normal,3,GL_FLOAT,GL_FALSE,0,0);

	glBindBuffer(GL_ARRAY_BUFFER,mesh.idtcr);
	glEnableVertexAttribArray(a_video_coord);
	glVertexAttribPointer(a_video_coord,3,GL_FLOAT,GL_FALSE,0,0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.idfce);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,video_frame_tex);

	glUniform1i(u_color_map,0);

	glDrawElements(GL_TRIANGLES,3*mesh.fcecount,GL_UNSIGNED_INT,0);

	glDisableVertexAttribArray(a_pos);
	glDisableVertexAttribArray(a_normal);
	glDisableVertexAttribArray(a_video_coord);

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
	// final
	rvideo_prog = loadProgram("vert_video.shd","frag_video.shd");

	a_pos = glGetAttribLocation(rvideo_prog,"a_pos");
	a_normal = glGetAttribLocation(rvideo_prog,"a_normal");
	a_video_coord = glGetAttribLocation(rvideo_prog,"a_video_coord");
	u_mvpmat = glGetUniformLocation(rvideo_prog,"u_mvpmat");
	u_color_map = glGetUniformLocation(rvideo_prog,"u_color_map");
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

void initGeometry(void)
{
	LoadObjFile(mesh,"video_mesh.obj");

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

void initTextures(void)
{
	glGenTextures(1,&video_frame_tex);
	glBindTexture(GL_TEXTURE_2D,video_frame_tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,video_tex_size,video_tex_size,0,GL_RGBA,GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}


static int open_codec_context(int *stream_idx,
							  AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret;
	AVStream *st;
	AVCodecContext *dec_ctx = nullptr;
	AVCodec *dec = nullptr;
	AVDictionary *opts = nullptr;
	ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
				av_get_media_type_string(type), src_filename);
		return ret;
	} else {
		*stream_idx = ret;
		st = fmt_ctx->streams[*stream_idx];
		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n",
					av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}
		/* Init the decoders, with or without reference counting */
		av_dict_set(&opts, "refcounted_frames", "1", 0);

		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
	}
	return 0;
}

void initVideoSource(void)
{
	int ret = 0;

	/* register all formats and codecs */
	av_register_all();
	/* open input file, and allocate format context */
	assert(avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) >= 0);
	/* retrieve stream information */
	assert(avformat_find_stream_info(fmt_ctx, NULL) >= 0);

	if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_dec_ctx = video_stream->codec;
		/* allocate image where the decoded image will be put */
		ret = av_image_alloc(video_dst_data, video_dst_linesize,
							 video_dec_ctx->width, video_dec_ctx->height,
							 video_dec_ctx->pix_fmt, 1);
		assert(ret>0);
		
		video_dst_bufsize = ret;
	}

	/* dump input information to s_textderr */
	av_dump_format(fmt_ctx, 0, src_filename, 0);
	if (!audio_stream && !video_stream) {
		fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
		ret = 1;
		assert(0);
	}
	/* When using the new API, you need to use the libavutil/frame.h API, while
	 * the classic frame management is available in libavcodec */
	frame = av_frame_alloc();
	assert(frame!=nullptr);

	/* initialize packet, set data to NULL, let the demuxer fill it */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	img_convert_ctx = sws_getContext(video_dec_ctx->width,video_dec_ctx->height,video_dec_ctx->pix_fmt,
									 video_tex_size, video_tex_size, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	//Allocate video frame for 24bit RGB that we convert to.
	frame_rgb = av_frame_alloc();
	assert(frame_rgb!=nullptr);
	uint8_t *buffer = new uint8_t[video_tex_size*video_tex_size*3];
	avpicture_fill((AVPicture*)frame_rgb, &buffer[0], PIX_FMT_RGB24, video_tex_size, video_tex_size);
}

int updateVideoFrame(void)
{
	int ret = 0;
	int decoded = pkt.size;
	got_frame = 0;
	if (pkt.stream_index == video_stream_idx) {
		/* decode video frame */
		ret = avcodec_decode_video2(video_dec_ctx, frame, &got_frame, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error decoding video frame ()\n");//, av_err2str(ret));
			return ret;
		}
		if (got_frame) {
			printf("video_frame%s n:%d coded_n:%d pts:\n",
				   cached ? "(cached)" : "",
				   video_frame_count++, frame->coded_picture_number//,
				   /*av_ts2timestr(frame->pts, &video_dec_ctx->time_base)*/);
			/* copy decoded frame to destination buffer:
			 * this is required since rawvideo expects non aligned data */
			av_image_copy(video_dst_data, video_dst_linesize,
						  (const uint8_t **)(frame->data), frame->linesize,
						  video_dec_ctx->pix_fmt, video_dec_ctx->width, video_dec_ctx->height);
			/* write to rawvideo file */
			// fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);

			sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, video_dec_ctx->height, frame_rgb->data, frame_rgb->linesize);

			glBindTexture(GL_TEXTURE_2D,video_frame_tex);
			glTexSubImage2D(GL_TEXTURE_2D,0,0,0,/*video_tex_size*/480,video_tex_size,GL_RGB,GL_UNSIGNED_BYTE,video_dst_data[0]);
			glTexSubImage2D(GL_TEXTURE_2D,0,0,272,/*video_tex_size*/480,video_tex_size,GL_RGB,GL_UNSIGNED_BYTE,video_dst_data[0]);
			// glTexSubImage2D(GL_TEXTURE_2D,0,,0,/*video_tex_size*/480,video_tex_size,GL_RGB,GL_UNSIGNED_BYTE,video_dst_data[0]);
			cout<<"Frame ("<<video_dec_ctx->width<<":"<<video_dec_ctx->height<<") - "<<video_dst_bufsize<<"b"<<endl;
		}
	}

	if (got_frame)
		av_frame_unref(frame);
	return decoded;
}

void freeVideoSource(void)
{
	avcodec_close(video_dec_ctx);
	avcodec_close(audio_dec_ctx);
	avformat_close_input(&fmt_ctx);

	av_frame_free(&frame);
	av_free(video_dst_data[0]);
}