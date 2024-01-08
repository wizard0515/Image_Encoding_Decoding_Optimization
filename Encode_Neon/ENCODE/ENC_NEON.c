
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <arm_neon.h>
#include "jtypes.h"
#include "jglobals.h"
#include "jtables.h"

WORD width_original = 1280,height_original = 960;

//功能：计算时间函数，放在被测函数执行前后，返回值为函数执行的时间（us）
double sub_time(struct timeval t1 , struct timeval t0)
{
    double s = t1.tv_sec - t0.tv_sec;
    double us = t1.tv_usec - t0.tv_usec;

    return s*1000000 + us;
}

//功能：向jpeg文件写入APP0段数据，应用程序保留标记0
void write_APP0info()
//Nothing to overwrite for APP0info
{
	writeword(APP0info.marker);
	writeword(APP0info.length);
	writebyte('J');
	writebyte('F');
	writebyte('I');
	writebyte('F');
	writebyte(0);
	writebyte(APP0info.versionhi);
	writebyte(APP0info.versionlo);
	writebyte(APP0info.xyunits);
	writeword(APP0info.xdensity);
	writeword(APP0info.ydensity);
	writebyte(APP0info.thumbnwidth);
	writebyte(APP0info.thumbnheight);
}

//功能： 写入SOF0段数据，Start of Frame，帧图像开始
void write_SOF0info()
{
	writeword(SOF0info.marker);
	writeword(SOF0info.length);
	writebyte(SOF0info.precision);
	writeword(SOF0info.height);
	writeword(SOF0info.width);
	writebyte(SOF0info.nrofcomponents);
	writebyte(SOF0info.IdY);
	writebyte(SOF0info.HVY);
	writebyte(SOF0info.QTY);
	writebyte(SOF0info.IdCb);
	writebyte(SOF0info.HVCb);
	writebyte(SOF0info.QTCb);
	writebyte(SOF0info.IdCr);
	writebyte(SOF0info.HVCr);
	writebyte(SOF0info.QTCr);
}

//功能：向jpeg文件写入DQT段量化表数据
void write_DQTinfo()
{
	BYTE i;
	
	writeword(DQTinfo.marker);
	writeword(DQTinfo.length);
	writebyte(DQTinfo.QTYinfo);
	for (i=0; i<64; i++) 
		writebyte(DQTinfo.Ytable[i]);
	writebyte(DQTinfo.QTCbinfo);
	for (i=0; i<64; i++) 
		writebyte(DQTinfo.Cbtable[i]);
}

//功能：设置量化表数据，并按zigzag顺序扫描，生成新的量化表
/*
	basic_table 输入的量化表
	scale_factor  量化参数，决定了图片压缩的质量
	newtable 经过计算后新的量化表
*/
void set_quant_table(BYTE *basic_table, BYTE scale_factor, BYTE *newtable)
{
	
	BYTE i;
	long temp;

	for (i=0; i<64; i++) 
	{
		temp = ((long) basic_table[i] * scale_factor + 50L) / 100L;
		// 限制量化表内参数的范围
		if (temp <= 0L) 
			temp = 1L;
		if (temp > 255L) 
			temp = 255L; 
		newtable[zigzag[i]] = (BYTE) temp;

	}
	
}

//功能：设置写入JPEG文件流的DQT量化表信息
//由JPEG标准提供的量化表，按量化参数，生成新的量化表，并写入JPEG文件流
void set_DQTinfo()
{
	BYTE scalefactor = 50;// scalefactor controls the visual quality of the image
	// the smaller is the better image we'll get, and the smaller 
	// compression we'll achieve
	DQTinfo.marker = 0xFFDB;
	DQTinfo.length = 132;
	DQTinfo.QTYinfo = 0;
	DQTinfo.QTCbinfo = 1;
	set_quant_table(std_luminance_qt, scalefactor, DQTinfo.Ytable);
	set_quant_table(std_chrominance_qt, scalefactor, DQTinfo.Cbtable);
}

////功能：设置写入JPEG文件流的DHT量化表信息
//写入了四张表的信息，YDC YAC CBAC CRAC
void write_DHTinfo()
{
	BYTE i;
	
	writeword(DHTinfo.marker);
	writeword(DHTinfo.length);
	writebyte(DHTinfo.HTYDCinfo);
	for (i=0; i<16; i++)  
		writebyte(DHTinfo.YDC_nrcodes[i]);
	for (i=0; i<12; i++) 
		writebyte(DHTinfo.YDC_values[i]);
	writebyte(DHTinfo.HTYACinfo);
	for (i=0; i<16; i++)
		writebyte(DHTinfo.YAC_nrcodes[i]);
	for (i=0; i<162; i++) 
		writebyte(DHTinfo.YAC_values[i]);
	writebyte(DHTinfo.HTCbDCinfo);
	for (i=0; i<16; i++) 
		writebyte(DHTinfo.CbDC_nrcodes[i]);
	for (i=0; i<12; i++)
		writebyte(DHTinfo.CbDC_values[i]);
	writebyte(DHTinfo.HTCbACinfo);
	for (i=0; i<16; i++)
		writebyte(DHTinfo.CbAC_nrcodes[i]);
	for (i=0; i<162; i++)
		writebyte(DHTinfo.CbAC_values[i]);
}

void set_DHTinfo()
{
	BYTE i;
	
	// 设置DHT段结构，写入的是JPEG标准推荐的Huffman表
	DHTinfo.marker = 0xFFC4;
	DHTinfo.length = 0x01A2;
	DHTinfo.HTYDCinfo = 0;
	for (i=0; i<16; i++)
		DHTinfo.YDC_nrcodes[i] = std_dc_luminance_nrcodes[i+1];
	for (i=0; i<12; i++)
		DHTinfo.YDC_values[i] = std_dc_luminance_values[i];
	
	DHTinfo.HTYACinfo = 0x10;
	for (i=0; i<16; i++)
		DHTinfo.YAC_nrcodes[i] = std_ac_luminance_nrcodes[i+1];
	for (i=0; i<162; i++)
		DHTinfo.YAC_values[i] = std_ac_luminance_values[i];
	
	DHTinfo.HTCbDCinfo = 1;
	for (i=0; i<16; i++)
		DHTinfo.CbDC_nrcodes[i] = std_dc_chrominance_nrcodes[i+1];
	for (i=0; i<12; i++)
		DHTinfo.CbDC_values[i] = std_dc_chrominance_values[i];
	
	DHTinfo.HTCbACinfo = 0x11;
	for (i=0; i<16; i++)
		DHTinfo.CbAC_nrcodes[i] = std_ac_chrominance_nrcodes[i+1];
	for (i=0; i<162; i++)
		DHTinfo.CbAC_values[i] = std_ac_chrominance_values[i];
}
//写入SOS段，Start of Scan，扫描开始 12字节
void write_SOSinfo()
{
	writeword(SOSinfo.marker);
	writeword(SOSinfo.length);
	writebyte(SOSinfo.nrofcomponents);
	writebyte(SOSinfo.IdY);
	writebyte(SOSinfo.HTY);
	writebyte(SOSinfo.IdCb);
	writebyte(SOSinfo.HTCb);
	writebyte(SOSinfo.IdCr);
	writebyte(SOSinfo.HTCr);
	writebyte(SOSinfo.Ss);
	writebyte(SOSinfo.Se);
	writebyte(SOSinfo.Bf);
}

void write_comment(BYTE *comment)
{
	WORD i, length;
	writeword(0xFFFE); 
	length = strlen((const char *)comment);
	writeword(length + 2);
	for (i=0; i<length; i++) 
		writebyte(comment[i]);
}
//功能：写入有限位的数据
//遇到OXFF这种特殊情况需要判断
//传入参数：结构体bitstring ，写入bs.length长度的bitstring.value
void writebits(bitstring bs)

{
	WORD value;
	SBYTE posval;
	//我们在bit流要读的位 范围为1-15
	value = bs.value;
	posval = bs.length - 1;
	while (posval >= 0)
	{
		if (value & mask[posval]) 
			bytenew |= mask[bytepos];
		posval--;
		bytepos--;
		if (bytepos < 0) 
		{ 
			
			if (bytenew == 0xFF) 
			{
				
				writebyte(0xFF);
				writebyte(0);
			}
			else 
				writebyte(bytenew);
			
			//重新设置
			bytepos = 7;
			bytenew = 0;
		}
	}
}
//功能：根据JPEG标准的Huffman表数据，计算最后熵编码用到的Huffman表，一共计算四张表
//values: 码长
//nrcodes：编码长度为x的数量
//HT:生成的Huffman表，按bitstring结构体存储
void compute_Huffman_table(BYTE *nrcodes, BYTE *std_table, bitstring *HT)
{
	BYTE k,j;
	BYTE pos_in_table;
	WORD codevalue;
	
	codevalue = 0; 
	pos_in_table = 0;
	for (k=1; k<=16; k++)
	{
		for (j=1; j<=nrcodes[k]; j++) 
		{
			HT[std_table[pos_in_table]].value = codevalue;
			HT[std_table[pos_in_table]].length = k;
			pos_in_table++;
			codevalue++;
		}
		
		codevalue <<= 1;
	}
}
//功能：调用compute_Huffman_table函数计算四张huffman表
void init_Huffman_tables()
{
	// Compute the Huffman tables used for encoding
	compute_Huffman_table(std_dc_luminance_nrcodes, std_dc_luminance_values, YDC_HT);
	compute_Huffman_table(std_ac_luminance_nrcodes, std_ac_luminance_values, YAC_HT);
	compute_Huffman_table(std_dc_chrominance_nrcodes, std_dc_chrominance_values, CbDC_HT);
	compute_Huffman_table(std_ac_chrominance_nrcodes, std_ac_chrominance_values, CbAC_HT);
}
//功能：输出错误信息
void exitmessage(char *error_message)
{
	printf("%s\n",error_message);
	exit(EXIT_FAILURE);
}
//功能：为之后熵编码提供对应关系
//category[]和bitcode[]由set_numbers_category_and_bitcode函数创建，
//category[i]表示i对应的二进制码长，HTDC[category[i]]表示i对应的码字，
//bitcode[i]表示i对应的二进制编码
void set_numbers_category_and_bitcode()
{
	SDWORD nr;
	SDWORD nrlower, nrupper;
	BYTE cat;

	category_alloc = (BYTE *)malloc(65535*sizeof(BYTE));
	if (category_alloc == NULL) 
		exitmessage("Not enough memory.");

	//允许负的变量
	category = category_alloc + 32767; 
	
	bitcode_alloc=(bitstring *)malloc(65535*sizeof(bitstring));
	if (bitcode_alloc==NULL) 
		exitmessage("Not enough memory.");
	bitcode = bitcode_alloc + 32767;
	
	nrlower = 1;
	nrupper = 2;
	for (cat=1; cat<=15; cat++) 
	{
		//正数
		for (nr=nrlower; nr<nrupper; nr++)
		{ 
			category[nr] = cat;
			bitcode[nr].length = cat;
			bitcode[nr].value = (WORD)nr;
		}
		//负数
		for (nr=-(nrupper-1); nr<=-nrlower; nr++)
		{ 
			category[nr] = cat;
			bitcode[nr].length = cat;
			bitcode[nr].value = (WORD)(nrupper-1+nr);
		}

		nrlower <<= 1;
		nrupper <<= 1;
	}
}
//功能：完成RGB_TO_YCBCR之前，先计算中间变量，以减小运算
void precalculate_YCbCr_tables()
{
	WORD R,G,B;

	for (R=0; R<256; R++) 
	{
		YRtab[R] = (SDWORD)(65536*0.299+0.5)*R;
		CbRtab[R] = (SDWORD)(65536*-0.16874+0.5)*R;
		CrRtab[R] = (SDWORD)(32768)*R;
	}
	for (G=0; G<256; G++) 
	{
		YGtab[G] = (SDWORD)(65536*0.587+0.5)*G;
		CbGtab[G] = (SDWORD)(65536*-0.33126+0.5)*G;
		CrGtab[G] = (SDWORD)(65536*-0.41869+0.5)*G;
	}
	for (B=0; B<256; B++) 
	{
		YBtab[B] = (SDWORD)(65536*0.114+0.5)*B;
		CbBtab[B] = (SDWORD)(32768)*B;
		CrBtab[B] = (SDWORD)(65536*-0.08131+0.5)*B;
	}
}

//功能：为减小DCT运算量，AAN算法将部分操作移植在量化因子上，需要计算量化表
void prepare_quant_tables()
{
	double aanscalefactor[8] = {1.0, 1.387039845, 1.306562965, 1.175875602,
		1.0, 0.785694958, 0.541196100, 0.275899379};
	BYTE row, col;
	BYTE i = 0;
	
	for (row = 0; row < 8; row++)
	{
		for (col = 0; col < 8; col++)
		{
			fdtbl_Y[i] = (float) (1.0 / ((double) DQTinfo.Ytable[zigzag[i]] *
				aanscalefactor[row] * aanscalefactor[col] * 8.0));
			fdtbl_Cb[i] = (float) (1.0 / ((double) DQTinfo.Cbtable[zigzag[i]] *
				aanscalefactor[row] * aanscalefactor[col] * 8.0));
			i++;
		}
	}
}


//AAN蝶形运算优化后的DCT
//data：输入的图像数据
//fdtbl:量化表
//outdata：量化和DCT变换后的数据
/*首先对转秩8x8数组，再将数据存入寄存器从而满足蝶形运算对应关系，
行运算后，使用两次vtrn转秩函数对寄存器数据重排，之后存入数组
完成行运算后再进行列运算，再将结果数据重新写回数组。*/
void fdct_and_quantization_NEON(signed char *data, float *fdtbl ,SWORD *outdata)
{
	float32x4_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	float32x4_t tmp10, tmp11, tmp12, tmp13;
	float32x4_t z1, z2, z3, z4, z5, z11, z13;
    float32x4_t t0 ,t1 ,t2 ,t3 ,t4 ,t5 ,t6 ,t7 ,t8;
    float32x4_t dst0 ,dst1 ,dst2 ,dst3 ,dst4 ,dst5 ,dst6 ,dst7 ,dst8;
    float *dataptr;
	float datafloat[64];
	float temp[64];
	float temp11;
    float32x4_t tmp15;
    float64x2_t tmp16;
	SBYTE ctr;
	BYTE i;

    float32x4x4_t st0 ,st1;

    for(int j = 0; j<8;j++)
    {
        for(int i=0;i<8;i++)
        {
            datafloat[i+8*j] =  data[j+8*i];
        }  
     }
	
    //处理行阶段
    dataptr = datafloat;
    t0 = vld1q_f32(dataptr);
    t1 = vld1q_f32(dataptr + 8);
    t2 = vld1q_f32(dataptr + 16);
    t3 = vld1q_f32(dataptr + 24);

    t4 = vld1q_f32(dataptr + 32);
    t5 = vld1q_f32(dataptr + 40);
    t6 = vld1q_f32(dataptr + 48);
    t7 = vld1q_f32(dataptr + 56);

    tmp0 = vaddq_f32(t0 ,t7);
    tmp7 = vsubq_f32(t0 ,t7);

    tmp1 = vaddq_f32(t1 ,t6);
    tmp6 = vsubq_f32(t1 ,t6);

    tmp2 = vaddq_f32(t2 ,t5);
    tmp5 = vsubq_f32(t2 ,t5);

    tmp3 = vaddq_f32(t3 ,t4);
    tmp4 = vsubq_f32(t3 ,t4);

    tmp10 = vaddq_f32(tmp0 ,tmp3);
    tmp13 = vsubq_f32(tmp0 , tmp3);

    tmp11 = vaddq_f32(tmp1 , tmp2);
    tmp12 = vsubq_f32(tmp1 , tmp2);

    st0.val[0] = vaddq_f32(tmp10 , tmp11); //dataptr[0]
    st1.val[0] = vsubq_f32(tmp10 , tmp11); //dataptr[4]
 
    z1 = vmulq_f32(vaddq_f32(tmp12 , tmp13) , vdupq_n_f32(0.707106781));
    
    st0.val[2] = vaddq_f32(tmp13 , z1);
    st1.val[2] = vsubq_f32(tmp13 , z1);


    tmp10 = vaddq_f32(tmp4 ,tmp5);
    tmp11 = vaddq_f32(tmp5 ,tmp6);
    tmp12 = vaddq_f32(tmp6 ,tmp7);

    z5 = vmulq_f32(vsubq_f32(tmp10 , tmp12) , vdupq_n_f32(0.382683433));
    z2 = vmlaq_f32(z5 , tmp10 , vdupq_n_f32(0.541196100));
    z4 = vmlaq_f32(z5 , tmp12 , vdupq_n_f32(1.306562965));
    z3 = vmulq_f32(tmp11 ,  vdupq_n_f32(0.707106781));

    z11 = vaddq_f32(tmp7 , z3);
    z13 = vsubq_f32(tmp7 , z3);

    st1.val[1] = vaddq_f32(z13 , z2);
    st0.val[3] = vsubq_f32(z13 , z2);
    st0.val[1] = vaddq_f32(z11 , z4);
    st1.val[3] = vsubq_f32(z11 , z4);

    tmp15 = st0.val[0];
    st0.val[0] = vtrn1q_f32(st0.val[0] ,st0.val[1]);
    st0.val[1] = vtrn2q_f32(tmp15 ,st0.val[1]);

    tmp15 = st0.val[2];
    st0.val[2] = vtrn1q_f32(st0.val[2] ,st0.val[3]);
    st0.val[3] = vtrn2q_f32(tmp15 ,st0.val[3]);

    tmp16 = vreinterpretq_f64_f32(st0.val[0]);
    st0.val[0] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st0.val[0]), vreinterpretq_f64_f32(st0.val[2])));
    st0.val[2] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st0.val[2])));

    tmp16 = vreinterpretq_f64_f32(st0.val[1]);
    st0.val[1] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st0.val[1]), vreinterpretq_f64_f32(st0.val[3])));
    st0.val[3] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st0.val[3])));

    tmp15 = st1.val[0];
    st1.val[0] = vtrn1q_f32(st1.val[0] ,st1.val[1]);
    st1.val[1] = vtrn2q_f32(tmp15 ,st1.val[1]);

    tmp15 = st1.val[2];
    st1.val[2] = vtrn1q_f32(st1.val[2] ,st1.val[3]);
    st1.val[3] = vtrn2q_f32(tmp15 ,st1.val[3]);

    tmp16 = vreinterpretq_f64_f32(st1.val[0]);
    st1.val[0] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st1.val[0]), vreinterpretq_f64_f32(st1.val[2])));
    st1.val[2] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st1.val[2])));

    tmp16 = vreinterpretq_f64_f32(st1.val[1]);
    st1.val[1] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st1.val[1]), vreinterpretq_f64_f32(st1.val[3])));
    st1.val[3] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st1.val[3])));

    for(int i=0; i<4; i++)
    {
        vst1q_f32(temp+8*i , st0.val[i]);
        vst1q_f32(temp+8*i+4 , st1.val[i]);
    }
    
    
    dataptr = datafloat+4;
    t0 = vld1q_f32(dataptr);
    t1 = vld1q_f32(dataptr + 8);
    t2 = vld1q_f32(dataptr + 16);
    t3 = vld1q_f32(dataptr + 24);

    t4 = vld1q_f32(dataptr + 32);
    t5 = vld1q_f32(dataptr + 40);
    t6 = vld1q_f32(dataptr + 48);
    t7 = vld1q_f32(dataptr + 56);

    tmp0 = vaddq_f32(t0 ,t7);
    tmp7 = vsubq_f32(t0 ,t7);

    tmp1 = vaddq_f32(t1 ,t6);
    tmp6 = vsubq_f32(t1 ,t6);

    tmp2 = vaddq_f32(t2 ,t5);
    tmp5 = vsubq_f32(t2 ,t5);

    tmp3 = vaddq_f32(t3 ,t4);
    tmp4 = vsubq_f32(t3 ,t4);

    tmp10 = vaddq_f32(tmp0 ,tmp3);
    tmp13 = vsubq_f32(tmp0 , tmp3);

    tmp11 = vaddq_f32(tmp1 , tmp2);
    tmp12 = vsubq_f32(tmp1 , tmp2);

    st0.val[0] = vaddq_f32(tmp10 , tmp11); //dataptr[0]
    st1.val[0] = vsubq_f32(tmp10 , tmp11); //dataptr[4]
 
    z1 = vmulq_f32(vaddq_f32(tmp12 , tmp13) , vdupq_n_f32(0.707106781));
    
    st0.val[2] = vaddq_f32(tmp13 , z1);
    st1.val[2] = vsubq_f32(tmp13 , z1);


    tmp10 = vaddq_f32(tmp4 ,tmp5);
    tmp11 = vaddq_f32(tmp5 ,tmp6);
    tmp12 = vaddq_f32(tmp6 ,tmp7);

    z5 = vmulq_f32(vsubq_f32(tmp10 , tmp12) , vdupq_n_f32(0.382683433));
    z2 = vmlaq_f32(z5 , tmp10 , vdupq_n_f32(0.541196100));
    z4 = vmlaq_f32(z5 , tmp12 , vdupq_n_f32(1.306562965));
    z3 = vmulq_f32(tmp11 ,  vdupq_n_f32(0.707106781));

    z11 = vaddq_f32(tmp7 , z3);
    z13 = vsubq_f32(tmp7 , z3);

    st1.val[1] = vaddq_f32(z13 , z2);
    st0.val[3] = vsubq_f32(z13 , z2);
    st0.val[1] = vaddq_f32(z11 , z4);
    st1.val[3] = vsubq_f32(z11 , z4);

    tmp15 = st0.val[0];
    st0.val[0] = vtrn1q_f32(st0.val[0] ,st0.val[1]);
    st0.val[1] = vtrn2q_f32(tmp15 ,st0.val[1]);

    tmp15 = st0.val[2];
    st0.val[2] = vtrn1q_f32(st0.val[2] ,st0.val[3]);
    st0.val[3] = vtrn2q_f32(tmp15 ,st0.val[3]);

    tmp16 = vreinterpretq_f64_f32(st0.val[0]);
    st0.val[0] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st0.val[0]), vreinterpretq_f64_f32(st0.val[2])));
    st0.val[2] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st0.val[2])));

    tmp16 = vreinterpretq_f64_f32(st0.val[1]);
    st0.val[1] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st0.val[1]), vreinterpretq_f64_f32(st0.val[3])));
    st0.val[3] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st0.val[3])));

    tmp15 = st1.val[0];
    st1.val[0] = vtrn1q_f32(st1.val[0] ,st1.val[1]);
    st1.val[1] = vtrn2q_f32(tmp15 ,st1.val[1]);

    tmp15 = st1.val[2];
    st1.val[2] = vtrn1q_f32(st1.val[2] ,st1.val[3]);
    st1.val[3] = vtrn2q_f32(tmp15 ,st1.val[3]);

    tmp16 = vreinterpretq_f64_f32(st1.val[0]);
    st1.val[0] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st1.val[0]), vreinterpretq_f64_f32(st1.val[2])));
    st1.val[2] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st1.val[2])));

    tmp16 = vreinterpretq_f64_f32(st1.val[1]);
    st1.val[1] = vreinterpretq_f32_f64(vtrn1q_f64(vreinterpretq_f64_f32(st1.val[1]), vreinterpretq_f64_f32(st1.val[3])));
    st1.val[3] = vreinterpretq_f32_f64(vtrn2q_f64(tmp16, vreinterpretq_f64_f32(st1.val[3])));

    for(int i=4; i<8; i++)
    {
        vst1q_f32(temp+8*i , st0.val[i-4]);
        vst1q_f32(temp+8*i+4 , st1.val[i-4]);
    }

    dataptr = temp;
    t0 = vld1q_f32(dataptr);
    t1 = vld1q_f32(dataptr + 8);
    t2 = vld1q_f32(dataptr + 16);
    t3 = vld1q_f32(dataptr + 24);

    t4 = vld1q_f32(dataptr + 32);
    t5 = vld1q_f32(dataptr + 40);
    t6 = vld1q_f32(dataptr + 48);
    t7 = vld1q_f32(dataptr + 56);

    tmp0 = vaddq_f32(t0 ,t7);
    tmp7 = vsubq_f32(t0 ,t7);

    tmp1 = vaddq_f32(t1 ,t6);
    tmp6 = vsubq_f32(t1 ,t6);

    tmp2 = vaddq_f32(t2 ,t5);
    tmp5 = vsubq_f32(t2 ,t5);

    tmp3 = vaddq_f32(t3 ,t4);
    tmp4 = vsubq_f32(t3 ,t4);

    tmp10 = vaddq_f32(tmp0 ,tmp3);
    tmp13 = vsubq_f32(tmp0 , tmp3);

    tmp11 = vaddq_f32(tmp1 , tmp2);
    tmp12 = vsubq_f32(tmp1 , tmp2);

    st0.val[0] = vaddq_f32(tmp10 , tmp11); //dataptr[0]
    st1.val[0] = vsubq_f32(tmp10 , tmp11); //dataptr[4]
 
    z1 = vmulq_f32(vaddq_f32(tmp12 , tmp13) , vdupq_n_f32(0.707106781));
    
    st0.val[2] = vaddq_f32(tmp13 , z1);
    st1.val[2] = vsubq_f32(tmp13 , z1);


    tmp10 = vaddq_f32(tmp4 ,tmp5);
    tmp11 = vaddq_f32(tmp5 ,tmp6);
    tmp12 = vaddq_f32(tmp6 ,tmp7);

    z5 = vmulq_f32(vsubq_f32(tmp10 , tmp12) , vdupq_n_f32(0.382683433));
    z2 = vmlaq_f32(z5 , tmp10 , vdupq_n_f32(0.541196100));
    z4 = vmlaq_f32(z5 , tmp12 , vdupq_n_f32(1.306562965));
    z3 = vmulq_f32(tmp11 ,  vdupq_n_f32(0.707106781));

    z11 = vaddq_f32(tmp7 , z3);
    z13 = vsubq_f32(tmp7 , z3);

    st1.val[1] = vaddq_f32(z13 , z2);
    st0.val[3] = vsubq_f32(z13 , z2);
    st0.val[1] = vaddq_f32(z11 , z4);
    st1.val[3] = vsubq_f32(z11 , z4);


    for(int i=0; i<4; i++)
    {
        vst1q_f32(temp+8*i , st0.val[i]);
        vst1q_f32(temp+8*(i+4) , st1.val[i]);
    }

    dataptr = temp + 4;
    t0 = vld1q_f32(dataptr);
    t1 = vld1q_f32(dataptr + 8);
    t2 = vld1q_f32(dataptr + 16);
    t3 = vld1q_f32(dataptr + 24);

    t4 = vld1q_f32(dataptr + 32);
    t5 = vld1q_f32(dataptr + 40);
    t6 = vld1q_f32(dataptr + 48);
    t7 = vld1q_f32(dataptr + 56);

    tmp0 = vaddq_f32(t0 ,t7);
    tmp7 = vsubq_f32(t0 ,t7);

    tmp1 = vaddq_f32(t1 ,t6);
    tmp6 = vsubq_f32(t1 ,t6);

    tmp2 = vaddq_f32(t2 ,t5);
    tmp5 = vsubq_f32(t2 ,t5);

    tmp3 = vaddq_f32(t3 ,t4);
    tmp4 = vsubq_f32(t3 ,t4);

    tmp10 = vaddq_f32(tmp0 ,tmp3);
    tmp13 = vsubq_f32(tmp0 , tmp3);

    tmp11 = vaddq_f32(tmp1 , tmp2);
    tmp12 = vsubq_f32(tmp1 , tmp2);

    st0.val[0] = vaddq_f32(tmp10 , tmp11); //dataptr[0]
    st1.val[0] = vsubq_f32(tmp10 , tmp11); //dataptr[4]
 
    z1 = vmulq_f32(vaddq_f32(tmp12 , tmp13) , vdupq_n_f32(0.707106781));
    
    st0.val[2] = vaddq_f32(tmp13 , z1);
    st1.val[2] = vsubq_f32(tmp13 , z1);


    tmp10 = vaddq_f32(tmp4 ,tmp5);
    tmp11 = vaddq_f32(tmp5 ,tmp6);
    tmp12 = vaddq_f32(tmp6 ,tmp7);

    z5 = vmulq_f32(vsubq_f32(tmp10 , tmp12) , vdupq_n_f32(0.382683433));
    z2 = vmlaq_f32(z5 , tmp10 , vdupq_n_f32(0.541196100));
    z4 = vmlaq_f32(z5 , tmp12 , vdupq_n_f32(1.306562965));
    z3 = vmulq_f32(tmp11 ,  vdupq_n_f32(0.707106781));

    z11 = vaddq_f32(tmp7 , z3);
    z13 = vsubq_f32(tmp7 , z3);

    st1.val[1] = vaddq_f32(z13 , z2);
    st0.val[3] = vsubq_f32(z13 , z2);
    st0.val[1] = vaddq_f32(z11 , z4);
    st1.val[3] = vsubq_f32(z11 , z4);


    for(int i=0; i<4; i++)
    {
        vst1q_f32(temp+8*i +4 , st0.val[i]);
        vst1q_f32(temp+8*(i+4) + 4 , st1.val[i]);
    }

	for(int i =0 ; i<16 ;i++)
	{
		tmp15 = vld1q_f32(temp + 4*i);
		tmp0  = vld1q_f32(fdtbl + 4*i);
		vst1q_f32( temp + 4*i ,vmulq_f32(tmp15 , tmp0));
	}



   
    for (i = 0; i < 64; i++) 
	{
	
	/* Round to nearest integer.
	   Since C does not specify the direction of rounding for negative
	   quotients, we have to force the dividend positive for portability.
	   The maximum coefficient size is +-16K (for 12-bit data), so this
	   code should work for either 16-bit or 32-bit ints. 
	*/
		outdata[i] = (SWORD) ((SWORD)(temp[i] + 16384.5) - 16384);
	}

}

//功能：将RGB数据转换为YCBCR数据NEON优化
//使用vst1_u8分离数据
//xpos 和 ypos为图片具体像素点位置，一次取64数据8x8
void load_data_units_from_RGB_buffer_NEON(WORD xpos, WORD ypos)
{
	BYTE x, y;
	BYTE pos = 0;
	DWORD location;
	BYTE R, G, B;
	uint8x8x3_t result;
	BYTE r_buffer[8] , g_buffer[8] , b_buffer[8];
	location = ypos * width + xpos;
	for (y=0; y<8; y++)
	{
		BYTE *temp = (BYTE *)(RGB_buffer + location);
		result = vld3_u8(temp);
			
		vst1_u8(r_buffer , result.val[2]);
		vst1_u8(g_buffer , result.val[1]);
		vst1_u8(b_buffer , result.val[0]);
		
		for(int i = 0 ; i <8;i++)
		{
			YDU[pos] = Y(r_buffer[i],g_buffer[i],b_buffer[i]);
			CbDU[pos] =Cb(r_buffer[i],g_buffer[i],b_buffer[i]);
			CrDU[pos] = Cr(r_buffer[i],g_buffer[i],b_buffer[i]);			
			pos++;
		}
		location += width;
	}
}

// 功能：neon优化后的process_DU对每个8x8单元的编码所有操作
// 应用了优化后的AAN DCT
//ComponentDU：输入的8x8数据
//fdtbl量化表
//DC：直流系数，用于差分编码
//HTDC直流熵编码Huffman表
//HTAC：交流熵编码Huffman表
void process_DU_NEON(SBYTE *ComponentDU,float *fdtbl,SWORD *DC, 
				bitstring *HTDC,bitstring *HTAC)
{
	bitstring EOB = HTAC[0x00];
	bitstring M16zeroes = HTAC[0xF0];
	BYTE i;
	BYTE startpos;
	BYTE end0pos;
	BYTE nrzeroes;
	BYTE nrmarker;
	SWORD Diff;

	fdct_and_quantization_NEON(ComponentDU, fdtbl, DU_DCT);
	
	// Z字扫描
	for (i=0; i<64; i++) 
		DU[zigzag[i]]=DU_DCT[i];
	 
	// 直流系数编码
	Diff = DU[0] - *DC;
	*DC = DU[0];
	
	//写入直流编码后的数据
	if (Diff == 0) 
		writebits(HTDC[0]); //Diff might be 0
	else 
	{
		writebits(HTDC[category[Diff]]);
		writebits(bitcode[Diff]);
	}
	
	// 交流编码
	for (end0pos=63; (end0pos>0)&&(DU[end0pos]==0); end0pos--) ;
	//记录最后一个非0数据的位置

   //交流Huffman编码并写入数据

	i = 1;
	while (i <= end0pos)
	{
		startpos = i;
		for (; (DU[i]==0) && (i<=end0pos); i++) ;
		nrzeroes = i - startpos;
		if (nrzeroes >= 16) 
		{
			for (nrmarker=1; nrmarker<=nrzeroes/16; nrmarker++) 
				writebits(M16zeroes);
			nrzeroes = nrzeroes%16;
		}
		writebits(HTAC[nrzeroes*16+category[DU[i]]]);
		writebits(bitcode[DU[i]]);
		i++;
	}
	//写入EOB（后面全是0）
	if (end0pos != 63) 
		writebits(EOB);
}


//优化后主编码函数
//功能：调用load_data_units_from_RGB_buffer_NEON色彩空间转换
//功能：调用process_DU_NEON对整个图片编码
void main_encoder_NEON()
{
	SWORD DCY = 0, DCCb = 0, DCCr = 0; //DC coefficients used for differential encoding
	WORD xpos, ypos;
	struct timeval t1 ,t0;
    static int st = 1;
	for (ypos=0; ypos<height; ypos+=8)
	{
		for (xpos=0; xpos<width; xpos+=8)
		{
	        load_data_units_from_RGB_buffer_NEON(xpos, ypos);
			process_DU_NEON(YDU, fdtbl_Y, &DCY, YDC_HT, YAC_HT);			
			process_DU_NEON(CbDU, fdtbl_Cb, &DCCb, CbDC_HT, CbAC_HT);			
			process_DU_NEON(CrDU, fdtbl_Cb, &DCCr, CbDC_HT, CbAC_HT);
		
		}
	}
}

// 优化后YUYVtoRGB函数
//色彩空间转换，将yuyv转换为RGB
//yuyvdata：存储着yuyv数据的数组
//rgbdata：转换后存储RGB数据的数组
//w图像长 h图像高
void YUYVtoRGB_Intrinsic(unsigned char* pNV12, unsigned char* pRGB, int width, int height)
{
	short int dataint[32];
	int16x8x4_t temp;
	int16x8_t r1 , g1 , b1 , r2 , g2 ,b2;
	int16x8_t c_34 = vdupq_n_s16(133);
	int16x8_t c_57 = vdupq_n_s16(227);
	int16x8_t c_23 = vdupq_n_s16(44);
	int16x8_t c_11 = vdupq_n_s16(91);
	int16x8x2_t r_change , b_change , g_change;
	int k =0;
    for (int j=0 ; j<(width * height * 2); j+=32)
	{
		
		//设置j
		//将32个数据，16个像素点的信息传入dataint数组，并扩展成16位
		for(int i = 0; i<32 ;i++)
		{
			dataint[i] = pNV12[i+j]; 
		}


		//交叉存储
		//result0 存 Y0 Y2 Y4 Y6		
		//result1 存 U0 U2 U4 U6
		//result2 存 Y1 Y3 Y5 Y7
		//result3 存 V1 V3 V5 V7
		//每个存储8个16位元素
		temp = vld4q_s16(dataint);

		temp.val[1] = vsubq_s16(temp.val[1] , vdupq_n_s16(128));
		temp.val[3] = vsubq_s16(temp.val[3] , vdupq_n_s16(128));
		//(uv - 128)
		int16x8_t r_temp = vshrq_n_s16( 
										(vmulq_s16(temp.val[3] , c_34)) , 7);
		r1 = vaddq_s16(temp.val[0] , r_temp);
		r2 = vaddq_s16(temp.val[2] , r_temp);
		//r1 = Y0 + 1.042*(V1 - 128); 
		int16x8_t b_temp = vshrq_n_s16( 
										(vmulq_s16(temp.val[1] , c_57)) , 7);
		b1 = vaddq_s16(temp.val[0] ,b_temp);
		b2 = vaddq_s16(temp.val[2] ,b_temp);
		//b1 = Y0 + 1.772 * (U0 - 128);

		//g1 = Y0 - 0.34414 * (U0 - 128) - 0.71414*(V1 - 128); 
		int16x8_t c_temp = vshrq_n_s16( vaddq_s16(
										        vmulq_s16(temp.val[1] , c_11) , vmulq_s16(temp.val[3] , c_23)) , 7);
		g1 = vsubq_s16(temp.val[0] , c_temp);
		g2 = vsubq_s16(temp.val[2] , c_temp);

		r_change = vzipq_s16(r1 ,r2);
		b_change = vzipq_s16(b1 ,b2);
		g_change = vzipq_s16(g1 ,g2);

		int16x8_t max =  vdupq_n_s16(255);
		int16x8_t min = vdupq_n_s16(0);
		int8x8x2_t rr_change ,bb_change ,gg_change;
		
		rr_change.val[0] = vmovn_s16(vmaxq_s16( (vminq_s16(r_change.val[0] , max)) , min));
	    rr_change.val[1] = vmovn_s16(vmaxq_s16((vminq_s16(r_change.val[1] , max)) , min));

		bb_change.val[0] = vmovn_s16(vmaxq_s16((vminq_s16(b_change.val[0] , max)) , min));
	    bb_change.val[1] = vmovn_s16(vmaxq_s16((vminq_s16(b_change.val[1] , max)) , min));

		gg_change.val[0] = vmovn_s16(vmaxq_s16((vminq_s16(g_change.val[0] , max)) , min));
	    gg_change.val[1] = vmovn_s16(vmaxq_s16((vminq_s16(g_change.val[1] , max)) , min));

		int8x8x3_t rgb_1 , rgb_2;
		rgb_1.val[0] = bb_change.val[0];
		rgb_1.val[1] = gg_change.val[0];
		rgb_1.val[2] = rr_change.val[0];

		rgb_2.val[0] = bb_change.val[1];
		rgb_2.val[1] = gg_change.val[1];
		rgb_2.val[2] = rr_change.val[1];

		
		vst3_s8(pRGB + k , rgb_1);
		vst3_s8(pRGB + k +24, rgb_2);
		k = k + 48;
		
	}
	

}

//功能：调用YUYVtoRGB_Intrinsic，将摄像头输出的YUYV文件转换为rgb色彩空间
void load_bitmap(char *bitmap_name, WORD width_original, WORD height_original)
{
	WORD widthDiv8, heightDiv8; // closest multiple of 8 [ceil]
	BYTE nr_fillingbytes;//The number of the filling bytes in the BMP file
	 // (the dimension in bytes of a BMP line on the disk is divisible by 4)
	colorRGB lastcolor;
	WORD column;
	BYTE TMPBUF[256];
	WORD nrline_up, nrline_dn, nrline;
	WORD dimline;
	colorRGB *tmpline;
    struct timeval t1 ,t0;
	
	width = width_original;
	height = height_original;
	
	unsigned char* yuv_buffer = (unsigned char *)malloc(sizeof(unsigned char) * width * height*3);
	RGB_buffer = (colorRGB *)(malloc(3*width_original*width_original));

	if (RGB_buffer == NULL) 
		exitmessage("Not enough memory for the bitmap image.");

	FILE *fp_bitmap = fopen(bitmap_name,"rb");
	fread(yuv_buffer, 1, width*height*2, fp_bitmap);

	YUYVtoRGB_Intrinsic(yuv_buffer, (unsigned char*) RGB_buffer, width_original, height_original);

	fclose(fp_bitmap);
}


//功能：初始化编码所有准备工作
//设置段，Huffman信息，预计算Ycbcr，预计算量化表
void init_all()
{
	
	set_DQTinfo();
	set_DHTinfo();
	init_Huffman_tables();
	set_numbers_category_and_bitcode();
	precalculate_YCbCr_tables();
	prepare_quant_tables();
}


void main(int argc, char *argv[])
{
	struct timeval t1 ,t0;
	gettimeofday(&t0 , NULL);
	char BMP_filename[64];//yuv文件名称
	char JPG_filename[64];//jpeg文件名称
	
	  
	BYTE len_filename;
	bitstring fillbits;
	

	if (argc>1) 
	{
		strcpy(BMP_filename,argv[1]);
		if (argc>2) 
			strcpy(JPG_filename,argv[2]);
		else 
		{ 
			strcpy(JPG_filename, BMP_filename);
			len_filename=strlen(BMP_filename);
			strcpy(JPG_filename+(len_filename-3),"jpg");
		}
	}
	else 
		exitmessage("Syntax: enc fis.bmp [fis.jpg]");

	gettimeofday(&t0 , NULL);
	//转换色彩空间
	load_bitmap(BMP_filename, width_original, height_original);
	//打开jpeg文件流
	fp_jpeg_stream = fopen(JPG_filename,"wb");
	init_all();
	SOF0info.width = width_original;
	SOF0info.height = height_original;

	//开始写入各段数据
	writeword(0xFFD8); // SOI
	write_APP0info();
	write_DQTinfo();
	write_SOF0info();
	write_DHTinfo();
	write_SOSinfo();

	bytenew = 0; // current byte
	bytepos = 7; // bit position in this byte

	//测量主编码函数时间
	
	main_encoder_NEON();
	gettimeofday(&t1 , NULL);
    printf("basic time used NEON : %f ms \n" , sub_time(t1 , t0) / 1000);

	//写入结尾数据段，关闭文件标识符
	// he EOI marker
	if (bytepos >= 0) 
	{
		fillbits.length = bytepos + 1;
		fillbits.value = (1<<(bytepos+1)) - 1;
		writebits(fillbits);
	}
	writeword(0xFFD9); // EOI
	
	free(RGB_buffer);
	free(category_alloc);
	free(bitcode_alloc);
	fclose(fp_jpeg_stream);
}
