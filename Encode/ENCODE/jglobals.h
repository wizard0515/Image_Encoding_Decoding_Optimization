
static BYTE bytenew=0; // 写入jpeg文件中的byte
static SBYTE bytepos=7; // 字节中具体要写的位 (bytenew)
						//<=7 and >=0
//mask数组，作用为帮助写入bits						
static WORD mask[16]={1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768};

// Huffman表
static bitstring YDC_HT[12];
static bitstring CbDC_HT[12];
static bitstring YAC_HT[256];
static bitstring CbAC_HT[256];


static BYTE *category_alloc;
static BYTE *category; //保存着数据序列 -32767..32767
static bitstring *bitcode_alloc;
static bitstring *bitcode; // 二进制编码

//预计算 YCbCr->RGB以便更快的转换
static SDWORD YRtab[256],YGtab[256],YBtab[256];
static SDWORD CbRtab[256],CbGtab[256],CbBtab[256];
static SDWORD CrRtab[256],CrGtab[256],CrBtab[256];

//量化表
float fdtbl_Y[64];
float fdtbl_Cb[64]; //the same with the fdtbl_Cr[64]

//存储RGB数据的数组
colorRGB *RGB_buffer; 
WORD width, height;// 图片尺寸

static SBYTE YDU[64]; // 转换后的8x8数据Y分量
static SBYTE CbDU[64];// 转换后的8x8数据cb分量
static SBYTE CrDU[64];// 转换后的8x8数据cb分量
static SWORD DU_DCT[64]; //DCT 量化后8x8数据单元
static SWORD DU[64]; //zigzag后的8x8数据单元

FILE *fp_jpeg_stream; //jpeg文件流
