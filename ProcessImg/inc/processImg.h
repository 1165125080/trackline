#ifndef PROCESSIMG_H
#define PROCESSIMG_H

#include <stdint.h>
#include "ov7725.h"

extern uint32_t starttime;


enum
{
	LINE_STRAIGHT,//ֱ��//0
	LINE_TURN_LEFT_90,//1
	LINE_TURN_RIGHT_90,//2
	LINE_END,//3
	LINE_LOST_ERROR,//4
	LINE_MARK//5
};

#define REPORT_PACKAGE_HEAD 0x23

#pragma pack(push)
#pragma pack(1)

typedef struct
{
	uint8_t head;
	uint8_t frame_cnt;//֡����
	uint8_t linestate;
	float angle_error;
	//�������
	//                        0 degree
	//                         A
	//                         A
	//                         A
	//                         A
	//                         A
	//                         A
	//-90degree ����������������A������������������������ +90degree
	int16_t middle_error;
	//�������ߵľ���//��ֵ�������ڷɻ���ߣ����ɻ���Ҫ���ƣ�
	uint8_t checksum;
} report_package_type;


#pragma pack(pop)


uint8_t processImg(uint8_t ImgData[][IMG_WIDTH]);



#endif
