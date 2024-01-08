#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <arm_neon.h>
#include "jtypes.h"
#include "jglobals.h"
#include "jtables.h"

//编码函数，处理的是摄像头输出的YUYV文件
//处理流程，色彩空间变换、区块切割和采样、离散余弦变换、量化、
//zigzag编码
//DC/AC系数分离编码、Huffman算数编码，最终输出JPEG比特流
//图片大小设置 必须为8的整数倍
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
	BYTE scalefactor = 50;// 量化参数，决定了图片压缩的质量
	//参数越小，得到的图片质量越好
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
			
			// 重新设置
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
//LLM蝶形运算DCT
//data：输入的图像数据
//fdtbl:量化表
//outdata：量化和DCT变换后的数据
void fdct_and_quantization_LLM(SBYTE *data, float *fdtbl, short int *outdata)
{
    float s07 , d07 , s16 , d16 , s25 , d25 ,s34 , d34;
    float temp2_1 ,temp2_2 , temp2_3 , temp2_4 ,temp2_5 , temp2_6 ,temp2_7 ,temp2_8;
    float temp3_1 , temp3_2 , temp3_3 , temp3_4;
    float* dataptr;
    float datafloat[64];
    SBYTE ctr;
	BYTE i;
    //      cospi/16      sin             cos3*pi/16     sin
   // c[5] = { 0.98078528  ,0.195090322   , 0.831469612 , 0.555570233  }
    float c1 =  0.98078528;
    float c3 =  0.831469612;

    float s1 =  0.195090322;
    float s3 =  0.555570233;

    float c4 = 0.275899379;
    float s4 = 1.38703984;
	float temp;

	for (i=0; i<64; i++) 
	    datafloat[i] = data[i];

    dataptr = datafloat;
   //处理行
    for (ctr = 7; ctr >= 0; ctr--) 
	{
        //state1
        s07 =  dataptr[0] + dataptr[7];
		d07 =  dataptr[0] - dataptr[7];
        s16 =  dataptr[1] + dataptr[6];
		d16 =  dataptr[1] - dataptr[6];
		s25 =  dataptr[2] + dataptr[5];
		d25 =  dataptr[2] - dataptr[5];
		s34 =  dataptr[3] + dataptr[4];
		d34 =  dataptr[3] - dataptr[4];

        //state2 
        temp2_1 = s07 + s34;
        temp2_2 = s16 + s25;
        temp2_3 = s16 - s25;
        temp2_4 = s07 - s34;

        temp2_5 = d34 * c3 + d07 * s3;
        temp2_6 = d25 * c1 + d16 * s1;
        temp2_7 = -d25 * s1 + d16 * c1;
        temp2_8 = -d34 * s3 + d07 * c3;

        //state3 
        dataptr[0] = temp2_1 + temp2_2;
        dataptr[4] = temp2_1 - temp2_2;

        dataptr[2] = temp2_3 * c4  + temp2_4 * s4;
        dataptr[6] = -temp2_3 * s4  + temp2_4 * c4;

        temp3_1  =  temp2_5 + temp2_7;
        temp3_2  =  -temp2_6 + temp2_8;
        temp3_3  =  temp2_5 - temp2_7 ;
        temp3_4  =  temp2_6 + temp2_8;

        //state4 
        dataptr[7] = temp3_4 - temp3_1;
        dataptr[1] = temp3_1 + temp3_4;
        dataptr[3] = 1.414213562 * temp3_2;
        dataptr[5] = 1.414213562 * temp3_3;

        dataptr += 8;
    }

    //处理列
    dataptr = datafloat;
     for (ctr = 7; ctr >= 0; ctr--) 
	{
        //state1
        s07 =  dataptr[0] + dataptr[56];
		d07 =  dataptr[0] - dataptr[56];
        s16 =  dataptr[8] + dataptr[48];
		d16 =  dataptr[8] - dataptr[48];
		s25 =  dataptr[16] + dataptr[40];
		d25 =  dataptr[16] - dataptr[40];
		s34 =  dataptr[24] + dataptr[32];
		d34 =  dataptr[24] - dataptr[32];

        //state2 
        temp2_1 = s07 + s34;
        temp2_2 = s16 + s25;
        temp2_3 = s16 - s25;
        temp2_4 = s07 - s34;

        temp2_5 = d34 * c3 + d07 * s3;
        temp2_6 = d25 * c1 + d16 * s1;
        temp2_7 = -d25 * s1 + d16 * c1;
        temp2_8 = -d34 * s3 + d07 * c3;

        //state3 
        dataptr[0] = temp2_1 + temp2_2;
        dataptr[32] = temp2_1 - temp2_2;

        dataptr[16] = temp2_3 * c4  + temp2_4 * s4;
        dataptr[48] = -temp2_3 * s4  + temp2_4 * c4;

        temp3_1  =  temp2_5 + temp2_7;
        temp3_2  =  -temp2_6 + temp2_8;
        temp3_3  =  temp2_5 - temp2_7 ;
        temp3_4  =  temp2_6 + temp2_8;

        //state4 
        dataptr[56] = temp3_4 - temp3_1;
        dataptr[8] = temp3_1 + temp3_4;
        dataptr[24] = 1.414213562 * temp3_2;
        dataptr[40] = 1.414213562 * temp3_3;

        dataptr++;
    }


    for (i = 0; i < 64; i++) 
	{
	
		temp = datafloat[i] * fdtbl[i];
		outdata[i] = (SWORD) ((SWORD)(temp + 16384.5) - 16384);
    }
}

//AAN蝶形运算DCT
//data：输入的图像数据
//fdtbl:量化表
//outdata：量化和DCT变换后的数据
void fdct_and_quantization(SBYTE *data, float *fdtbl, SWORD *outdata)
{
	float tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	float tmp10, tmp11, tmp12, tmp13;
	float z1, z2, z3, z4, z5, z11, z13;
	float *dataptr;
	float datafloat[64];
	float temp;
	SBYTE ctr;
	BYTE i;

	for (i=0; i<64; i++) 
		datafloat[i] = data[i];

	/* 处理行 */
	dataptr = datafloat;
	for (ctr = 7; ctr >= 0; ctr--) 
	{
		tmp0 = dataptr[0] + dataptr[7];
		tmp7 = dataptr[0] - dataptr[7];
		tmp1 = dataptr[1] + dataptr[6];
		tmp6 = dataptr[1] - dataptr[6];
		tmp2 = dataptr[2] + dataptr[5];
		tmp5 = dataptr[2] - dataptr[5];
		tmp3 = dataptr[3] + dataptr[4];
		tmp4 = dataptr[3] - dataptr[4];

		

		tmp10 = tmp0 + tmp3;	/* phase 2 */
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		dataptr[0] = tmp10 + tmp11; /* phase 3 */
		dataptr[4] = tmp10 - tmp11;

		z1 = (tmp12 + tmp13) * ((float) 0.707106781); /* c4 */
		dataptr[2] = tmp13 + z1;	/* phase 5 */
		dataptr[6] = tmp13 - z1;

		

		tmp10 = tmp4 + tmp5;	/* phase 2 */
		tmp11 = tmp5 + tmp6;
		tmp12 = tmp6 + tmp7;

		
		z5 = (tmp10 - tmp12) * ((float) 0.382683433); /* c6 */
		z2 = ((float) 0.541196100) * tmp10 + z5; /* c2-c6 */
		z4 = ((float) 1.306562965) * tmp12 + z5; /* c2+c6 */
		z3 = tmp11 * ((float) 0.707106781); /* c4 */

		z11 = tmp7 + z3;		
		z13 = tmp7 - z3;

		dataptr[5] = z13 + z2;	
		dataptr[3] = z13 - z2;
		dataptr[1] = z11 + z4;
		dataptr[7] = z11 - z4;

		dataptr += 8;		
	}

  /*处理列 */

	dataptr = datafloat;
	for (ctr = 7; ctr >= 0; ctr--) 
	{
		tmp0 = dataptr[0] + dataptr[56];
		tmp7 = dataptr[0] - dataptr[56];
		tmp1 = dataptr[8] + dataptr[48];
		tmp6 = dataptr[8] - dataptr[48];
		tmp2 = dataptr[16] + dataptr[40];
		tmp5 = dataptr[16] - dataptr[40];
		tmp3 = dataptr[24] + dataptr[32];
		tmp4 = dataptr[24] - dataptr[32];

		/* Even part */

		tmp10 = tmp0 + tmp3;	
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		dataptr[0] = tmp10 + tmp11; 
		dataptr[32] = tmp10 - tmp11;

		z1 = (tmp12 + tmp13) * ((float) 0.707106781); /* c4 */
		dataptr[16] = tmp13 + z1; 
		dataptr[48] = tmp13 - z1;

		/* Odd part */

		tmp10 = tmp4 + tmp5;	
		tmp11 = tmp5 + tmp6;
		tmp12 = tmp6 + tmp7;

		
		z5 = (tmp10 - tmp12) * ((float) 0.382683433);
		z2 = ((float) 0.541196100) * tmp10 + z5; 
		z4 = ((float) 1.306562965) * tmp12 + z5;
		z3 = tmp11 * ((float) 0.707106781);

		z11 = tmp7 + z3;		/* phase 5 */
		z13 = tmp7 - z3;

		dataptr[40] = z13 + z2; /* phase 6 */
		dataptr[24] = z13 - z2;
		dataptr[8] = z11 + z4;
		dataptr[56] = z11 - z4;

		dataptr++;			/* 指针指向下一列*/
	}

	/*量化输出结果 */
	for (i = 0; i < 64; i++) 
	{
		
		temp = datafloat[i] * fdtbl[i];
	
	/*
	 取整为最近的整形
	*/
		outdata[i] = (SWORD) ((SWORD)(temp + 16384.5) - 16384);
	}
}



// 功能：process_DU对每个8x8单元的编码所有操作
//ComponentDU：输入的8x8数据
//fdtbl量化表
//DC：直流系数，用于差分编码
//HTDC直流熵编码Huffman表
//HTAC：交流熵编码Huffman表
void process_DU(SBYTE *ComponentDU,float *fdtbl,SWORD *DC, 
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

	fdct_and_quantization_LLM(ComponentDU, fdtbl, DU_DCT);
	
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



//功能：将RGB数据转换为YCBCR数据
//xpos 和 ypos为图片具体像素点位置，一次取64数据8x8
void load_data_units_from_RGB_buffer(WORD xpos, WORD ypos)
{
	BYTE x, y;
	BYTE pos = 0;
	DWORD location;
	BYTE R, G, B;

	location = ypos * width + xpos;
	for (y=0; y<8; y++)
	{
		for (x=0; x<8; x++)
		{
			R = RGB_buffer[location].R;
			G = RGB_buffer[location].G;
			B = RGB_buffer[location].B;
			// convert to YCbCr
			YDU[pos] = Y(R,G,B);
			CbDU[pos] = Cb(R,G,B);
			CrDU[pos] = Cr(R,G,B);			
			location++;
			pos++;
		}
		location += width - 8;
	}
}


//主编码函数
//功能：调用load_data_units_from_RGB_buffer色彩空间转换
//功能：调用process_DU对整个图片编码
void main_encoder()
{
	SWORD DCY = 0, DCCb = 0, DCCr = 0; //DC coefficients used for differential encoding
	WORD xpos, ypos;
	struct timeval t1 ,t0;
    static int st = 1;
	for (ypos=0; ypos<height; ypos+=8)
	{
		for (xpos=0; xpos<width; xpos+=8)
		{
	        load_data_units_from_RGB_buffer(xpos, ypos);
			process_DU(YDU, fdtbl_Y, &DCY, YDC_HT, YAC_HT);			
			process_DU(CbDU, fdtbl_Cb, &DCCb, CbDC_HT, CbAC_HT);			
			process_DU(CrDU, fdtbl_Cb, &DCCr, CbDC_HT, CbAC_HT);
		}
	}
}




//色彩空间转换，将yuyv转换为RGB
//yuyvdata：存储着yuyv数据的数组
//rgbdata：转换后存储RGB数据的数组
//w图像长 h图像高
void yuyv_to_rgb(unsigned char *yuyvdata, unsigned char *rgbdata, int w, int h)
{
   int r1, g1, b1; 
	int r2, g2, b2;
    for(int i=0; i<w*h/2; i++)
	{
	    char data[4];
	    memcpy(data, yuyvdata+i*4, 4);//一次拿四个数据,比如Y0 U0 Y1 V1 可以解析出两个像素点[Y0 U0 V1] [Y1 U0 V1]
	    unsigned char Y0=data[0];
        unsigned char U0=data[1];
	    unsigned char V1=data[3];
	    unsigned char Y1=data[2];
	     
		//Y0U0Y1V1  -->[Y0 U0 V1] [Y1 U0 V1]
	    r1 = Y0 + 1.042*(V1 - 128); if(r1>250)r1=255; if(r1<0)r1=0;
	    g1 = Y0 - 0.34414 * (U0 - 128) - 0.71414*(V1 - 128); if(g1>250)g1=255; if(g1<0)g1=0;
	    b1 = Y0 + 1.772 * (U0 - 128);  if(b1>250)b1=255; if(b1<0)b1=0;
	 
	    r2 = Y1 + 1.042 * (V1 - 128);if(r2>250)r2=255; if(r2<0)r2=0;
	    g2 = Y1 - 0.34414 * (U0 - 128) - 0.71414 * (V1 - 128); if(g2>250)g2=255; if(g2<0)g2=0;
	    b2 = Y1 + 1.772 * (U0 - 128);  if(b2>250)b2=255; if(b2<0)b2=0;
	    
	    rgbdata[i*6+0]=b1;
	    rgbdata[i*6+1]=g1;
	    rgbdata[i*6+2]=r1;
	    rgbdata[i*6+3]=b2;
	    rgbdata[i*6+4]=g2;
	    rgbdata[i*6+5]=r2;
	}
}


	
//功能：调用yuyv_to_rgb，将摄像头输出的YUYV文件转换为rgb色彩空间
void load_bitmap(char *bitmap_name, WORD width_original, WORD height_original)
{
	struct timeval t1 ,t0;
	WORD widthDiv8, heightDiv8; 
	BYTE nr_fillingbytes;
	colorRGB lastcolor;
	WORD column;
	BYTE TMPBUF[256];
	WORD nrline_up, nrline_dn, nrline;
	WORD dimline;
	colorRGB *tmpline;
   
	width = width_original;
	height = height_original;
	
	unsigned char* yuv_buffer = (unsigned char *)malloc(sizeof(unsigned char) * width * height*3);
	RGB_buffer = (colorRGB *)(malloc(3*width*height));

	if (RGB_buffer == NULL) 
		exitmessage("Not enough memory for the bitmap image.");

	FILE *fp_bitmap = fopen(bitmap_name,"rb");
	fread(yuv_buffer, 1, width*height*2, fp_bitmap);
	yuyv_to_rgb(yuv_buffer, (unsigned char*) RGB_buffer, width, height);
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

//程序主函数
void main(int argc, char *argv[])
{
	struct timeval t1 ,t0;
	char BMP_filename[64];//yuv文件名称
	char JPG_filename[64];//jpeg文件名称
	BYTE len_filename;
	bitstring fillbits; 
	
	//处理命令行传入的jpeg bmp文件名
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
	//测量编码函数时间
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

	
	bytenew = 0; 
	bytepos = 7;

    
   
	main_encoder();
	gettimeofday(&t1 , NULL);
    printf("basic time used: %f ms \n" , sub_time(t1 , t0) / 1000);
    
	
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
