#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/mman.h>
#include "font.h"

//封装在任意位置显示任意大小的bmp图片
int show_anybmp(int w,int h,int x,int y,const char *bmpname)
{
	int bmpfd;
	int lcdfd;
	int i,j;
	//定义int类型的指针指向lcd在显存中的首地址
	int *lcdmem;

	//定义数组存放像素点的RGB
	char bmpbuf[w*h*3];
	//定义数组存放转换得到的ARGB
	int lcdbuf[w*h]; // int占4字节
	int tempbuf[w*h];
	//打开你要显示的bmp图片   w*h
	bmpfd=open(bmpname,O_RDWR);
	if(bmpfd==-1)
	{
		perror("打开图片");
		return -1;
	}
	
	//打开lcd的驱动
	lcdfd=open("/dev/fb0",O_RDWR);
	if(lcdfd==-1)
	{
		perror("打开lcd");
		return -1;
	}
	
	//映射得到lcd在显存中对应的首地址
	lcdmem=(int *)mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcdfd,0);
	if(lcdmem==NULL)
	{
		perror("映射lcd");
		return -1;
	}
	
	//跳过前面没有用的54字节
	lseek(bmpfd,54,SEEK_SET);
	
	//判断bmp图片的宽所占的字节数能否被4整除
	if((w*3)%4!=0)
	{
		for(i=0; i<h; i++)
		{
			read(bmpfd,&bmpbuf[i*w*3],w*3);
			lseek(bmpfd,4-(w*3)%4,SEEK_CUR);  //跳过填充的垃圾数据
		}
	}
	else
		//从55字节读取bmp的像素点颜色值
		read(bmpfd,bmpbuf,w*h*3);  //bmpbuf[0] B  bmpbuf[1] G bmpbuf[2] R  一个像素点的RGB
	                               //bmpbuf[3]  bmpbuf[4]  bmpbuf[5] 
	//3字节的RGB-->4字节的ARGB   位运算+左移操作
	for(i=0; i<w*h; i++)
		lcdbuf[i]=bmpbuf[3*i]|bmpbuf[3*i+1]<<8|bmpbuf[3*i+2]<<16|0x00<<24;
	                   //00[2][1][0]
					   
	for(i=0; i<w; i++)
		for(j=0; j<h; j++)
			//*(lcdmem+(y+j)*800+x+i)=lcdbuf[j*w+i];  图片颠倒
			*(lcdmem+(y+j)*800+x+i)=lcdbuf[(h-1-j)*w+i];
	
	//关闭
	close(bmpfd);
	close(lcdfd);
	//解除映射
	munmap(lcdmem,800*480*4);
	return 0;
}

