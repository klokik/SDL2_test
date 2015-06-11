
#define T_TEXTURE		0x101
#define T_ARRAY_BUFFER	0x102


class GpuBuffer
{
public:
	uint32_t id;
	uint32_t type;
	uint32_t sA,sB,sC,sD;

	GpuBuffer(int _type=T_TEXTURE)
	{
		type=_type;

		if(_type==T_TEXTURE)
			glGenTextures(1,&id);
		else
		if(_type==T_ARRAY_BUFFER)
			glGenBuffers(1,&id);
	}

	void resize(int _sA,int _sB=1,int _sC=1,int _sD=1)
	{
		glBindBuffer(GL_ARRAY_BUFFER,id);
		glBufferData();
	}

	~GpuBuffer(void)
	{
		switch(type)
		{
		case T_TEXTURE:
			if(glIsTexture(id))
				glDeleteTextures(1,&id);
			break;
		case T_ARRAY_BUFFER:
			if(glIsBuffer(id))
				glDeleteBuffers(1,&id);
			break;
		}
	}
};