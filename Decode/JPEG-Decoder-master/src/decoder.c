# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <string.h>
# include <math.h>
# include <unistd.h>
# include <sys/time.h>



//定义标志位
# define M_HEAD 0xFF
// SOI, 图像开始标志
# define M_SOI  0xD8
// APP0, 应用程序保留标记0
# define M_APP0 0xE0
# define M_APP1 0xE1
# define M_APP2 0xE2
# define M_APP3 0xE3
# define M_APP4 0xE4
# define M_APP5 0xE5
# define M_APP6 0xE6
# define M_APP7 0xE7
# define M_APP8 0xE8
# define M_APP9 0xE9
# define M_APPA 0xEA
# define M_APPB 0xEB
# define M_APPC 0xEC
# define M_APPD 0xED
# define M_APPE 0xEE
# define M_APPF 0xEF
// DQT, 量化表开始标志位
# define M_DQT  0xDB
// SOF0, 帧图像开始标志
# define M_SOF0 0xC0
// DHT, 哈夫曼表开始标志
# define M_DHT  0xC4
// DRI, 定义重启间隔
# define M_DRI  0xDD
// SOS, 扫描开始标志
# define M_SOS  0xDA
// COM, 注释
# define M_COM  0xFE
// EOI, 图像结束标志
# define M_EOI  0xD9

# define N_DQT   16
# define N_QUANT 64
# define N_CHAN  5
# define N_DHT   16
# define N_HDEP  16

# define N_BLOCK 10

# define M_COS 200

# define TYPE_DC 0
# define TYPE_AC 1

# define FL_SKIP 1
# define FL_NSKIP 0

# define idx(i, j, h) (((i) * (h)) + (j))

# define check(cond, msg)           \
    do {                            \
        if (cond) {              \
            printf("%s\n", msg);    \
            exit(-1);               \
        }                           \
    } while(0);


//数据类型定义
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

typedef struct {
    u32 buffer;
    u8 len;
    u8 skip_fl;
    u32 counter;
    FILE* fp;
} F_BUFFER;

typedef struct {
    u8 precise;
    u16 quantizer[N_QUANT];
} DQT;

//图像通道定义
typedef struct {
    //水平方向反采样因子
    u8 h_factor;
    // 垂直方向反采样因子
    u8 v_factor;
    // 每个MCU中的图像块数
    u8 N_BLOCK_MCU;
    u8 DQT_ID;
    u8 DC_table;
    u8 AC_table;
} CHAN;

typedef struct H_NODE {
    struct H_NODE* l;
    struct H_NODE* r;
} H_NODE;

typedef struct {
    H_NODE* root;
    H_NODE* pt;
    u8 input;
    u8 len;
    u8 symbol;
    u8 status;
} DHT;

typedef struct JPEG_INFO_ {
    DQT* DQT[N_DQT];
    u8 precise;
    u16 height;
    u16 width;
    u8 n_color;
    u16 DRI;
    CHAN* CHAN[N_CHAN];
    DHT* DC_DHT[N_DHT];
    DHT* AC_DHT[N_DHT];
    F_BUFFER* fp;
    u8 MCU_v_factor;
    u8 MCU_h_factor;
    u8 MCU_NV;
    u8 MCU_NH;
    u16 N_MCU;
    u16 MCU_height;
    u16 MCU_width;
} JPEG_INFO;

typedef double BLOCK[N_QUANT];

// 数据处理单元
typedef struct {
    BLOCK** BLOCK;   // N_CHAN * N_BLOCK
} MCU;

typedef struct {
    MCU** MCU;
} JPEG_DATA;

typedef struct {
    u8* R;
    u8* G;
    u8* B;
    u16 width;
    u16 height;
} RGB;

// RGB文件格式定义
RGB* init_RGB (u16 width, u16 height) {

    RGB* obj = malloc(sizeof(RGB));
    obj->width  = width;
    obj->height = height;
    obj->R      = malloc(sizeof(u8) * width * height);
    obj->G      = malloc(sizeof(u8) * width * height);
    obj->B      = malloc(sizeof(u8) * width * height);
    return obj;
}

/*  
    函数功能：以字节为单元向BMP文件文件中写入图像数据
    函数输入：文件名；数据量
    函数输出：无
*/
void write_u32 (FILE* fp, u32 num) {
    // Write little-endian u32

    for (u8 i = 0; i < 4; ++i) {
        fprintf(fp, "%c", num & 0xFF);
        num >>= 8;
    }
    return;
}
/*  
    函数功能：以16位二进制位为单元向BMP文件文件中写入数据
    函数输入：文件名；数据量
    函数输出：无
*/
void write_u16 (FILE* fp, u16 num) {
    // Write little-endin u16

    for (u8 i = 0; i < 2; ++i) {
        fprintf(fp, "%c", num & 0xFF);
        num >>= 8;
    }
    return;
}
/*  
    函数功能：将图像数据写入BMP文件
    函数输入：文件名；图像文件
    函数输出：无
*/
void write_bmp (char* filename, RGB* img) {

    // 打开文件
    FILE* fp = fopen(filename, "w");
    check(fp == NULL, "Open BMP file fail");

    // 补位后的图像大小
    u32 img_size = img->height * ((img->width * 3 + 3) / 4) * 4;
    u32 padding_size = ((img->width * 3 + 3) / 4) * 4 - img->width * 3;

    
    fprintf(fp, "BM");              // Magic number
    write_u32(fp, 54 + img_size);   // File size, Header size = 54
    write_u32(fp, 0);               // Riverse 1 & 2
    write_u32(fp, 54);              // Offset

    // 写入文件头
    write_u32(fp, 40);              // Ifo header size
    write_u32(fp, img->width);      // Width
    write_u32(fp, img->height);     // Height
    write_u16(fp, 1);               // # planes
    write_u16(fp, 24);              // # bits per pixel
    write_u32(fp, 0);               // NO compression
    write_u32(fp, img_size);        // Image size
    write_u32(fp, 100);             // Horizontal resolution
    write_u32(fp, 100);             // Vertical resolution
    write_u32(fp, 0);               // Number of color palette
    write_u32(fp, 0);               // Nnumber of important colors

    // 写入像素数据
    for (i32 i = img->height - 1; i >= 0; --i) {
        for (u32 j = 0; j < img->width; ++j) {
            u32 index = idx(i, j, img->width);
            fprintf(fp, "%c%c%c", img->B[index], img->G[index], img->R[index]);
        }
        for (u16 j = 0; j < padding_size; ++j)
            fprintf(fp, "%c", 0);
    }
    fflush(fp);
    fclose(fp);

    return;
}
/*  
    函数功能：测试输出
    函数输入：图像文件
    函数输出：像素数据
*/
void write_ppm (RGB* img) {

    FILE* fp = fopen("test.ppm", "w");
    fprintf(fp, "P3\n%d %d\n255\n", img->width, img->height);
    for (u16 i = 0; i < img->height; ++i) {
        for (u16 j = 0; j < img->width; ++j) {
            u32 index = idx(i, j, img->width);
            fprintf(fp, "%d %d %d ", img->R[index], img->G[index], img->B[index]);
        }
        fprintf(fp, "\n");
    }
}

/*  
    函数功能：读入缓冲区数据
    函数输入：缓冲区名；数据长
    函数输出：无
*/
u8 read_BUFFER (u16* content, u8 len, F_BUFFER* buf) {


    check(len > 16, "Length more then 16!");

    if (buf->len >= len) {
        *content = (u16)((buf->buffer >> (buf->len - len)) & ((1 << len) - 1));
        buf->buffer  &= ((1 << (buf->len - len)) - 1);
        buf->len     -= len;
        buf->counter += len;
        return 1;
    }
    else {
        u8 c;
        if (!fread(&c, sizeof(u8), 1, buf->fp))
            return 0;
        buf->buffer = (buf->buffer << 8) + c;
        buf->len += 8;
        if (buf->skip_fl == FL_SKIP && c == 0xFF) {
            fread(&c, sizeof(u8), 1, buf->fp);
        }
        return read_BUFFER(content, len, buf);
    }
}
/*  
    函数功能：向缓冲区写入数据
    函数输入：缓冲区名；数据长
    函数输出：无
*/
void put_BUFFER (u8 content, u8 len, F_BUFFER* buf) {

    buf->buffer  += content << buf->len;
    buf->len     += len;
    buf->counter -= len;

}
/*  
    函数功能：从缓冲区读出8位数据到数组
    函数输入：缓冲区名
    函数输出：数组名
*/
u8 read_u8 (u8* content, F_BUFFER* buf) {
    // Read u8 from buffer

    u16 content16;
    u8 ret = read_BUFFER(&content16, 8, buf);
    *content = (u8)(content16);
    return ret;
}
/*  
    函数功能：从缓冲区读出16位数据到数组
    函数输入：缓冲区名
    函数输出：数组名
*/
u8 read_u16 (u16* content, F_BUFFER* buf) {

    return read_BUFFER(content, 16, buf);
}
/*  
    函数功能：计算当前数据长度
    函数输入：数组头指针
    函数输出：数据长
*/
u16 read_len (F_BUFFER* buf) {

    u16 len;
    check(!read_u16(&len, buf), "Wrong length!");
    return len;
}
/*  
    函数功能：计算已读数据字节数
    函数输入：数组头指针
    函数输出：数据长
*/
u32 get_counter (F_BUFFER* buf) {

    return (buf->counter) / 8;
}

void set_skip_fl (F_BUFFER* buf, u8 fl) {

    buf->skip_fl =fl;
    return;
}
/*  
    函数功能：初始化文件缓冲区
    函数输入：文件路径
    函数输出：缓冲区头指针
*/
F_BUFFER* init_F_BUFFER (char* file_path) {

    F_BUFFER* obj = malloc(sizeof(F_BUFFER));
    obj->fp       = fopen(file_path, "rb");
    obj->skip_fl  = FL_NSKIP;
    obj->len      = 0;
    obj->buffer   = 0;
    obj->counter  = 0;
    return obj;
}

/*  
    函数功能：检查哈弗曼树当前节点是否为叶子节点
    函数输入：根节点
    函数输出：0/1
*/
u8 H_NODE_is_leaf (H_NODE* root) {

    return root->l == root;
}
/*  
    函数功能：获取哈弗曼树叶子节点
    函数输入：根节点
    函数输出：叶子节点
*/
u8 H_NODE_symbol (H_NODE* root) {

    return (u8)(u64)root->r;
}
/*  
    函数功能：新建哈弗曼树节点
    函数输入：无
    函数输出：根节点
*/
H_NODE* H_NODE_init () {

    H_NODE* obj = malloc(sizeof(H_NODE));
    obj->l = NULL;
    obj->r = NULL;
    return obj;
}
/*  
    函数功能：在哈弗曼树中增加节点
    函数输入：节点值；根节点
    函数输出：无
*/
void H_NODE_add_symbol (H_NODE* root, u16 content, u8 len, u8 symbol) {
  
    // 检查根节点是否为叶子节点
    check(H_NODE_is_leaf(root), "Wrong tree construction!");

    // 增加元素
    if (len == 0) {
        root->l = root;
        root->r = (H_NODE*)(u64)symbol;
    }
    else {
        if ((content >> (len - 1)) & 1) {
            // 增加右侧节点
            if (root->r == NULL)
                root->r = H_NODE_init();
            H_NODE_add_symbol(root->r, content, len - 1, symbol);
        }
        else {
            // 增加左侧节点
            if (root->l == NULL)
                root->l = H_NODE_init();
            H_NODE_add_symbol(root->l, content, len - 1, symbol);
        }
    }
    return;
}

/*  
    函数功能：遍历二叉树
    函数输入：根节点
    函数输出：节点数
*/
void H_NODE_triverse (H_NODE* root, u16 content) {

    if (root->l == root) {
        printf("%x %d\n", content, (u8)(u64)root->r);
    }
    else {
        if (root->l)
            H_NODE_triverse(root->l, content << 1);
        else
            printf("LEFT-NULLLLLLLLLLLLLLLL\n");
        if (root->r)
            H_NODE_triverse(root->r, (content << 1) + 1);
        else
            printf("RIGHT-NULLLLLLLLLLLLLLLL\n");
    }
}

/*  
    函数功能：Huffman表初始化
    函数输入：无
    函数输出：表头指针
*/
DHT* init_DHT () {

    DHT* obj = malloc(sizeof(DHT));
    obj->root = H_NODE_init();
    return obj;
}

/*  
    函数功能：重置Huffman表
    函数输入：表头指针
    函数输出：无
*/
void DHT_reset(DHT* table) {

    table->len    = 0;
    table->pt     = table->root;
    table->status = 0;
}
/*  
    函数功能：遍历Huffman表
    函数输入：表头指针
    函数输出：0/1
*/
u8 DHT_run(DHT* table) {
    
    if (H_NODE_is_leaf(table->pt)) {
        // 找到叶子节点
        table->symbol = H_NODE_symbol(table->pt);
        table->pt     = table->root;
        table->status = 1;
        return 1;
    }
    else if (table->len == 0) { 
        // 无效元素
        table->status = 0;
        return 0;
    }
    else if (((table->input) >> (table->len - 1)) & 1)
        table->pt = table->pt->r;
    else  
        table->pt = table->pt->l;
    table->len--;
    return DHT_run(table);
}
/*  
    函数功能：Huffman表初始化
    函数输入：无
    函数输出：表头指针
*/
u8 DHT_put(DHT* table, u8 input, u8 len) {

    table->input = input;
    table->len     = len;
    return DHT_run(table);
}

u8 DHT_get_symbol (DHT* table) {
    // 从Huffman表中获取元素

    return table->symbol;
}

u8 DHT_get_input (DHT* table) {
    // 从Huffman表中获取输入

    return table->input;
}

u8 DHT_get_len (DHT* table) {
    // 获取Huffman表长度

    return table->len;
}

u8 DHT_get_status (DHT* table) {
    // 获取Huffman表状态

    return table->status;
}

/*  
    函数功能：利用Huffman表还原参数
    函数输入：Huffman表；数据缓冲区
    函数输出：0/1
*/
u8 get_symbol_len (DHT* DHT_table, F_BUFFER* buf) {

    // 重置Huffman表
    DHT_reset(DHT_table);
    while (!DHT_get_status(DHT_table)) {
        u8 c;
        read_u8(&c, buf);
        DHT_put(DHT_table, c, 8);
    }
    
    u8 remain_content = DHT_get_input(DHT_table);
    u8 remain_len = DHT_get_len(DHT_table);
    put_BUFFER(remain_content, remain_len, buf);
    return DHT_get_symbol(DHT_table);
}

i16 get_symbol (u8 len, F_BUFFER* buf) {
    // 获取缓冲区数据长

    i16 c;
    read_BUFFER((u16*)&c, len, buf);
    
    if (c >> (len - 1))
        return c;
    else 
        
        return -(c ^ ((1 << len) - 1));
}
/*  
    函数功能：Zigzag解码恢复
    函数输入：图像块数组
    函数输出：无
*/
void anti_zz (BLOCK obj) {

    u8 zz_table[8][8] = {
    { 0,  1,  5,  6, 14, 15, 27, 28 },
    { 2,  4,  7, 13, 16, 26, 29, 42 },
    { 3,  8, 12, 17, 25, 30, 41, 43 },
    { 9, 11, 18, 24, 31, 40, 44, 53 },
    { 10, 19, 23, 32, 39, 45, 52, 54 },
    { 20, 22, 33, 38, 46, 51, 55, 60 },
    { 21, 34, 37, 47, 50, 56, 59, 61 },
    { 35, 36, 48, 49, 57, 58, 62, 63 }
    };
    BLOCK *res = malloc(sizeof(BLOCK));
    memset(res, 0, sizeof(BLOCK));
    // 转换直流系数与交流系数
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            (*res)[i * 8 + j] = obj[zz_table[i][j]];
        }
    }
    
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            obj[i * 8 + j] = (*res)[i * 8 + j];
        }
    }
}

/*  
    函数功能：量化恢复
    函数输入：图像块数组；量化表
    函数输出：无
*/
void anti_q (BLOCK obj, DQT* quant) {
    
    for (int i = 0; i < 64; ++i) {
        // 8位量化系数
        if (quant->precise == 0) {
            obj[i] *= (quant->quantizer[i] & 0x00FF);
        }
        // 16位量化系数
        else if (quant->precise == 1) {
            obj[i] *= quant->quantizer[i];
        }
    }

}

/*  
    函数功能：IDCT解码
    函数输入：图像块数组
    函数输出：无
*/
void fst_IDCT (BLOCK des, BLOCK src) {

    static double* cos_table = NULL;
    static double* coeff     = NULL;

    // 初始余弦系数
    if (!cos_table) {
        cos_table = malloc(sizeof(double) * M_COS);
        for (u8 i = 0; i < M_COS; i++)
            cos_table[i] = cos(i * M_PI / 16.0);
    }

    // 初始常量系数
    if (!coeff) {
        coeff = malloc(sizeof(double) * 8);
        coeff[0] = 1. / sqrt(2.);
        for (u8 i = 1; i < 8; i++)
            coeff[i] = 1.;
    }

    // 余弦变换
    memset(des, 0, sizeof(double) * N_QUANT);
    for (int j = 0; j < 8; j++)
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++)
                des[idx(j, x, 8)] += coeff[y] * src[idx(x, y, 8)] * cos_table[((j << 1) + 1) * y];
            des[idx(j, x, 8)] /= 2.;
        }
    return;
}

void IDCT (BLOCK obj) {
    // 离散余弦反变换

    BLOCK tmp;
    fst_IDCT(tmp, obj);
    fst_IDCT(obj, tmp);
}

/*  
    函数功能：对YUV数据分别采样还原
    函数输入：图像块数组
    函数输出：无
*/
void upsampling(MCU* obj, JPEG_INFO* info) {

    for (u8 chan = 1; chan <= 3; ++chan) {
        // 采样比例未变化，无需操作
        if ((info->CHAN[chan]->v_factor == info->MCU_v_factor) && \
            (info->CHAN[chan]->h_factor == info->MCU_h_factor))
            continue;
        u8 MCU_N_BLOCK = info->MCU_v_factor * info->MCU_h_factor;
        BLOCK* up[MCU_N_BLOCK];
        for (u8 i = 0; i < MCU_N_BLOCK; ++i)
            up[i] = malloc(sizeof(BLOCK));

        for (u8 i = 0; i < info->MCU_v_factor; ++i) {
            for (u8 j = 0; j < info->MCU_h_factor; ++j) {
                for (u8 k = 0; k < 8; ++k) {
                    for (u8 l = 0; l < 8; ++l) {
                        u8 x = (8 * i + k) * info->CHAN[chan]->v_factor / info->MCU_v_factor;
                        u8 y = (8 * j + l) * info->CHAN[chan]->h_factor / info->MCU_h_factor;
                        (*up[info->MCU_h_factor * i + j])[8 * k + l] = \
                            (*obj->BLOCK[idx(chan, (x / 8) * info->CHAN[chan]->h_factor + (y / 8), N_CHAN)])\
                            [8 * (x % 8) + (y % 8)];
                    }
                }
            }
        }
        for (u8 i = 0; i < MCU_N_BLOCK; ++i)
            obj->BLOCK[idx(chan, i, N_CHAN)] = up[i];
    }
}

/*  
    函数功能：色彩带通滤波器，滤去0~255之外的数据
    函数输入：图像数据
    函数输出：滤波结果
*/
u8 bandpass (double val) {

    if (val >= 255.0)
        return 255;
    else if (val <= 0.0)
        return 0;
    else
        return (u8) round(val);
}
/*  
    函数功能：色彩空间变换，YUV->RGB
    函数输入：图像数据
    函数输出：变换结果
*/
RGB* anti_trans_color (JPEG_INFO* info, JPEG_DATA* data) {

    RGB* img = init_RGB(info->width, info->height);
    for (u8 i = 0; i < info->MCU_NV; ++i)
        for (u8 j = 0; j < info->MCU_NH; ++j)
            for (u8 k = 0; k < info->MCU_v_factor; ++k)
                for (u8 l = 0; l < info->MCU_h_factor; ++l)
                    for (u8 m = 0; m < 8; ++m)
                        for (u8 n = 0; n < 8; ++n) {
                            u16 x = 8 * info->MCU_v_factor * i + 8 * k + m;
                            u16 y = 8 * info->MCU_h_factor * j + 8 * l + n;
                            if (x >= info->height || y >= info->width)
                                continue;
                            u16 idx_MCU   = info->MCU_NH * i + j;
                            u16 idx_BLOCK = info->MCU_h_factor * k + l;
                            u16 idx_px    = 8 * m + n;
                            u32 index     = idx(x, y, info->width);
                            double Y  = (*data->MCU[idx_MCU]->BLOCK[idx(1, idx_BLOCK, N_CHAN)])[idx_px];
                            double C1 = (*data->MCU[idx_MCU]->BLOCK[idx(2, idx_BLOCK, N_CHAN)])[idx_px];
                            double C2 = (*data->MCU[idx_MCU]->BLOCK[idx(3, idx_BLOCK, N_CHAN)])[idx_px];
                            img->R[index] = bandpass(1.0 * Y + 1.402 * C2 + 128.0);
                            img->G[index] = bandpass(1.0 * Y - 0.34414 * C1 - 0.71414 * C2 + 128.0);
                            img->B[index] = bandpass(1.0 * Y + 1.772 * C1 + 128.0);
                        }
    return img;
}

/*  
    函数功能：读入图像数据块
    函数输入：图像数据，通道标号
    函数输出：存储区头指针
*/
BLOCK* read_BLOCK (JPEG_INFO* info, u8 chan_id) {
    
    DHT* DC_table = info->DC_DHT[info->CHAN[chan_id]->DC_table];
    DHT* AC_table = info->AC_DHT[info->CHAN[chan_id]->AC_table];
    BLOCK* obj = malloc(sizeof(BLOCK));
    memset(obj, 0, sizeof(BLOCK));
    u8 symbol_len;
    // 初始化直流偏移量
    static i16 DC_bias[N_CHAN] = {0};   

    // 读取直流系数
    symbol_len = get_symbol_len(DC_table, info->fp);
    (*obj)[0] = get_symbol(symbol_len, info->fp) + DC_bias[chan_id];
    DC_bias[chan_id] = (*obj)[0];

    // 读取交流系数
    u8 zeros, idx = 1;
    while (idx < 64) {
        symbol_len = get_symbol_len(AC_table, info->fp);
        if (symbol_len == 0x00) {
            // 读取到文件结束标志位
            while (idx < 64) {
                (*obj)[idx++] = 0;
            }
        }
        else if (symbol_len == 0xF0) {
            for (int i = 0; i < 16; ++i) {
                (*obj)[idx++] = 0;
            }
        }
        else {
            //计算0的个数
            zeros = symbol_len >> 4;
            for (int i = 0; i < zeros; ++i) {
                (*obj)[idx++] = 0;
            }
            // 处理非0的AC系数
            (*obj)[idx++] = get_symbol((symbol_len & 0x0F), info->fp);
        }
    }
    
    return obj;
}

/*  
    函数功能：读取一个MCU并进行编码操作
    函数输入：图像数据
    函数输出：存储区头指针
*/
MCU* read_MCU (JPEG_INFO* info) {

    MCU* obj = malloc(sizeof(MCU));
    obj->BLOCK = malloc(sizeof(BLOCK*) * N_CHAN * N_BLOCK);
    for (u8 chan = 1; chan <= 3; ++chan) {
        for (u8 i = 0; i < info->CHAN[chan]->N_BLOCK_MCU; ++i) {
            u16 index = idx(chan, i, N_CHAN);
            // Huffman解码
            obj->BLOCK[index] = read_BLOCK(info, chan);
            // 量化恢复
            anti_q((double*)obj->BLOCK[index], info->DQT[info->CHAN[chan]->DQT_ID]);
            // Zigzag编码恢复
            anti_zz((double*)obj->BLOCK[index]);
            IDCT((double*)obj->BLOCK[index]);
        }
    }
    upsampling(obj, info);
    return obj;
}
/*  
    函数功能：读取JPEG图片，转换为RGB信息的图像
    函数输入：图像数据
    函数输出：RGB图像
*/
RGB* read_JPEG_img (JPEG_INFO* info) {

    JPEG_DATA obj;
    obj.MCU = malloc(sizeof(MCU*) * info->N_MCU);
    set_skip_fl(info->fp, FL_SKIP);
    for (u16 i = 0; i < info->N_MCU; ++i) {
        obj.MCU[i] = read_MCU(info);
    }
    set_skip_fl(info->fp, FL_NSKIP);
    info->fp->len = (info->fp->len / 8) * 8;
    RGB* img = anti_trans_color(info, &obj);
    return img;
}

JPEG_INFO* init_JPEG_INFO (char* jpeg_path) {
    // 初始化JPEG存储空间
    JPEG_INFO* obj = malloc(sizeof(JPEG_INFO));
    obj->fp           = init_F_BUFFER(jpeg_path);
    obj->DRI          = 0;
    obj->MCU_v_factor = 0;
    obj->MCU_h_factor = 0;
    for (int i = 0;i < N_DQT;++i)
        obj->DQT[i] = NULL;
    for (int i = 0;i < N_CHAN;++i)
        obj->CHAN[i] = NULL;
    return obj;
}

void read_APP0 (JPEG_INFO* info) {
    // 读取APP0后的数据

    u16 len = read_len(info->fp);
    u8 c;
    
    for (int i = 0; i < len - 2; ++i)
        read_u8(&c, info->fp);
    return;
}

void read_APPn (JPEG_INFO* info) {
    // 读取APPn后的数据

    u16 len = read_len(info->fp);
    u8 c;

    for (int i = 0; i < len - 2; ++i)
        read_u8(&c, info->fp);
    return;
}
/*  
    函数功能：读取量化表信息
    函数输入：图像数据
    函数输出：无
*/
void read_DQT (JPEG_INFO* info) {

    u8 val;
    u16 len, repeat;
    
    read_u16(&len, info->fp);
    repeat = (len - 2) / (N_QUANT + 1);
    check(repeat * (N_QUANT + 1) + 2 != len, "Wrong DQT size");

    for (u16 i = 0; i < repeat; ++i) {
        read_u8(&val, info->fp);
        DQT* obj = malloc(sizeof(DQT));
        obj->precise = val >> 4;
        for (int j = 0; j < N_QUANT; ++j)
            if (obj->precise == 0) 
                read_u8((u8*)(obj->quantizer + j), info->fp);
            else 
                read_u16(obj->quantizer + j, info->fp);
        info->DQT[val & 0x0F] = obj;
    }
}

void read_SOF0 (JPEG_INFO* info) {
    // Read SOF0

    u8 id;
    // 2 bytes长度数据
    u16 len = read_len(info->fp);
    // 1 bytes精度数据
    read_u8(&(info->precise), info->fp);
    // 2 bytes高度数据
    read_u16(&(info->height), info->fp);
    // 2 bytes宽度数据
    read_u16(&(info->width), info->fp);
    // 1 bytes通道标号
    read_u8(&(info->n_color), info->fp);

    // 检查数据长度
    check(8 + 3 * info->n_color != len, "Wrong SOF0 size");

    // 读取色块数据
    for (int i = 0; i < info->n_color; ++i) {
        CHAN* obj = malloc(sizeof(CHAN));
        read_u8(&id, info->fp);
        // 通道标号， Y: 1, Cb: 2, Cr: 3 
        info->CHAN[id] = obj;
        read_u8(&(obj->h_factor), info->fp);
        read_u8(&(obj->DQT_ID), info->fp);
        // 低四位表示反采样垂直因子
        obj->v_factor = obj->h_factor & 0x0F;
        // 高四位表示反采样水平因子
        obj->h_factor = obj->h_factor >> 4;
        obj->N_BLOCK_MCU = (obj->v_factor) * (obj->h_factor);
        if (obj->v_factor > info->MCU_v_factor)
            info->MCU_v_factor = obj->v_factor;
        if (obj->h_factor > info->MCU_h_factor)
            info->MCU_h_factor = obj->h_factor;
    }
    // 覆盖整个图像所需要的最少MCU数目
    info->MCU_NV = (info->height + info->MCU_v_factor * 8 - 1) / (info->MCU_v_factor * 8);
    info->MCU_NH = (info->width + info->MCU_h_factor * 8 - 1) / (info->MCU_h_factor * 8);
    info->N_MCU  = info->MCU_NV * info->MCU_NH;
    // 补位后的MCU长宽数据
    info->MCU_height = (info->MCU_v_factor) * (info->MCU_NV) * 8;
    info->MCU_width  = (info->MCU_h_factor) * (info->MCU_NH) * 8;
}
// 读取Huffman表
void read_DHT (JPEG_INFO* info) {
    

    u16 len = read_len(info->fp);
    len -= 2;

    while (1) {
        if (len == 0)
            break;
        check(len & 0x8000, "DHT len error!\n");
        DHT* obj = init_DHT();
        u16 counter = 0;
        u8 c_len = 0xFF;
        u16 content;
        u8 symbol;
        u8 num[N_HDEP];
        u8 c;


        read_u8(&c, info->fp);
        if (c >> 4 == TYPE_DC)
            info->DC_DHT[c & 0x0F] = obj;
        else if (c >> 4 == TYPE_AC)
            info->AC_DHT[c & 0x0F] = obj;

        for (int i = 0; i < N_HDEP; ++i) {
            read_u8(num + i, info->fp);
            counter += num[i];
            if (num[i] != 0 && c_len == 0xFF)
                c_len = i;
        }

        // 构建Huffman表对应的二叉树
        content = 0;
        for (int i = 0; i < counter; ++i) {
            read_u8(&symbol, info->fp);
            H_NODE_add_symbol(obj->root, content, c_len + 1, symbol);
            num[c_len]--;
            content++;
            while (!num[c_len] && i != counter - 1) {
                c_len++;
                content = content << 1;
            }
        }
        len = len - 17 - counter;

    }
}
//读取文件头中DRI标志位后的内容
void read_DRI (JPEG_INFO* info) {

    u16 len = read_len(info->fp);
    check(len != 4, "DRI len error!\n");
    read_u16(&(info->DRI), info->fp);
}
//读取文件头中COM标志位后的内容
void read_COM (JPEG_INFO* info) {

    u16 len = read_len(info->fp);
    u8 c;
    printf("===== Comment =====\n");
    for (u16 i = 0; i < len - 2; ++i) {
        read_u8(&c, info->fp);
        printf("%c", c);
    }
    printf("\n===================\n");
}
//读取文件头中的SOS标志位
void read_SOS (JPEG_INFO* info) {

    u8 tmp;
    u16 len = read_len(info->fp);
    len -= 2;


    read_u8(&(info->n_color), info->fp);
    for (int i = 0; i < (len - 4) / 2; ++i) {
        u8 ID;
        u8 table;
        read_u8(&ID, info->fp);
        read_u8(&table, info->fp);
        info->CHAN[ID]->DC_table = table >> 4;
        info->CHAN[ID]->AC_table = table & 0x0F;
    }

    read_u8(&tmp, info->fp);
    check(tmp != 0x00, "Wrong SOS-1\n");
    read_u8(&tmp, info->fp);
    check(tmp != 0x3F, "Wrong SOS-2\n");
    read_u8(&tmp, info->fp);
    check(tmp != 0x00, "Wrong SOS-3\n");
}
//读取文件头中的标志位
JPEG_INFO* read_JPEG_INFO (char* jpeg_path) {

    JPEG_INFO* info = init_JPEG_INFO(jpeg_path);
    u8 val_now;

    while (read_u8(&val_now, info->fp)) {
        if (val_now == M_HEAD) {
            read_u8(&val_now, info->fp);
            switch (val_now) {
                case M_SOI:
                    printf("SOI at %d\n", get_counter(info->fp));
                    break;
                case M_EOI:
                    printf("EOI at %d\n", get_counter(info->fp));
                    break;
                case M_APP0:
                    read_APP0(info);
                    printf("APP0 at %d\n", get_counter(info->fp));
                    break;
                case M_APP1:
                case M_APP2:
                case M_APP3:
                case M_APP4:
                case M_APP5:
                case M_APP6:
                case M_APP7:
                case M_APP8:
                case M_APP9:
                case M_APPA:
                case M_APPB:
                case M_APPC:
                case M_APPD:
                case M_APPE:
                case M_APPF:
                    read_APPn(info);
                    printf("APPn at %d\n", get_counter(info->fp));
                    break;
                case M_DQT:
                    read_DQT(info);
                    printf("DQT at %d\n", get_counter(info->fp));
                    break;
                case M_SOF0:
                    read_SOF0(info);
                    printf("SOF0 at %d\n", get_counter(info->fp));
                    break;
                case M_DHT:
                    read_DHT(info);
                    printf("DHT at %d\n", get_counter(info->fp));
                    break;
                case M_DRI:
                    read_DRI(info);
                    printf("DRI at %d\n", get_counter(info->fp));
                    break;
                case M_COM:
                    read_COM(info);
                    printf("COM at %d\n", get_counter(info->fp));
                    break;
                case M_SOS:
                    read_SOS(info);
                    printf("SOS at %d\n", get_counter(info->fp));
                    return info;
                case 0:
                default:
                    printf("Unknow marker at %d, %x\n", get_counter(info->fp), val_now);
            }
        }
        else {
            printf("Unknow byte at %d, val = %x\n", get_counter(info->fp), val_now);
        }
    }

    return info;
}
//获取当前系统时间
double sub_time(struct timeval t1 , struct timeval t0)
{
    double s = t1.tv_sec - t0.tv_sec;
    double us = t1.tv_usec - t0.tv_usec;

    return s*1000000 + us;
}
//主函数
int main (int argc, char* argv[]) {

    // 输入图像路径
	struct timeval t1 ,t0;
	gettimeofday(&t0 , NULL);
    if (argc !=3) {
        printf("Usage\n\t %s jpeg_path bmp_path\n", argv[0]);
        exit(1);
    }

    // 读取数据
    JPEG_INFO* info = read_JPEG_INFO(argv[1]);
    //图像解码
    RGB* img = read_JPEG_img(info);
    //写入数据
    write_bmp(argv[2], img);
    
    
    gettimeofday(&t1 , NULL);
    printf("basic time used : %f ms \n" , sub_time(t1 , t0) / 1000);
}
