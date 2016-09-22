#include "processImg.h"
#include "usart.h"

#define ABS(intnum) ((intnum >= 0)?(intnum):(-intnum))

static uint8_t reportnow(uint8_t linestate,float angle_error,int16_t middle_error);
static uint8_t sendtrace(uint8_t ImgData[][IMG_WIDTH]);
static uint8_t CameraGetTrack(uint8_t ImgData[][IMG_WIDTH]);//��ȡ����
static uint8_t CalTrack(void);//��������
static int16_t CalMiddleError(uint8_t state);
static float CalAngleError(uint8_t state);
static uint16_t findnearestest_1(uint8_t* LineData,int16_t startpoint);



uint8_t ifstarightline = 0;
int16_t top;
int16_t bottom;
uint32_t starttime;

typedef struct
{
	int16_t left;
	int16_t right;
} onerow_type;//û���ҵ���ʱleft��rightͬ��0
//left,rightӦ�ö��ں�������

onerow_type trace_result[IMG_HEIGHT];


//uint16_t histogram[0xff+1];//ֱ��ͼ����
uint8_t gatevalue; 

#include <stdio.h>
#include <string.h>
char processeddata[50] = "";


uint8_t processImg(uint8_t ImgData[][IMG_WIDTH])
{
//	//����һ����ֱ��ͼ����
//	for(uint16_t ii = 0; ii<=0xff;ii++)
//		histogram[ii]=0;
//	for(uint8_t ii = 0;ii < IMG_HEIGHT; ii ++)
//		for(uint8_t jj = 0;jj < IMG_WIDTH; jj ++)
//		{
//			histogram[ImgData[ii][jj]]++;
//		}
//	uint32_t pixadd;

	
	//�Ҷ�ֵ����ֵ
	#define GATEVALUEOFFSET 35
	uint32_t sumofgray = 0;
	for(uint16_t ii = 0;ii < IMG_HEIGHT; ii ++)
		for(uint16_t jj = 0;jj < IMG_WIDTH; jj ++)
			sumofgray += ImgData[ii][jj];
	gatevalue = sumofgray/(IMG_HEIGHT*IMG_WIDTH) - GATEVALUEOFFSET;
	

	
	CameraGetTrack(ImgData);
	//Ѱ�� end
	//��С���˷�����
	uint8_t state;
	state = CalTrack();
	int16_t middleerror;
	middleerror = CalMiddleError(state);
	float angleerror;
	angleerror = CalAngleError(state);
	//���ͼ����
	reportnow(state,angleerror,middleerror);
	sendtrace(ImgData);
	
	snprintf(processeddata,sizeof(processeddata),"\ns:%d\t m:%d\t a%d\t t%d\t",
		(int)state,(int)middleerror,(int)angleerror,(int)(HAL_GetTick() - starttime));
	HAL_UART_Transmit_DMA(&huart4,(uint8_t*)processeddata,strlen(processeddata));
	return 0;
}


report_package_type report_package;
static uint8_t reportnow(uint8_t linestate,float angle_error,int16_t middle_error)
{
	report_package.head = REPORT_PACKAGE_HEAD;
	report_package.frame_cnt++;//֡����
	report_package.linestate = linestate;
	report_package.angle_error = angle_error;
	report_package.middle_error = middle_error;
	report_package.checksum = 0;
	for(uint16_t ii;ii<sizeof(report_package)-1;ii++)
		report_package.checksum += *(((uint8_t*)(&report_package))+ii);
	if(HAL_UART_Transmit_DMA(&huart3,(uint8_t *)&report_package,sizeof(report_package)) == HAL_OK)//Ӧ������ʱ�䷢����
		return 0;
	else
		return 1;
}




struct
{
	uint8_t head;
	uint8_t buff[IMG_HEIGHT/2][IMG_WIDTH/2];//���ڴ��洦����
} tracereport;


static uint8_t sendtrace(uint8_t ImgData[][IMG_WIDTH])//����ʶ����ͼ��
{
	if((huart2.State == HAL_UART_STATE_READY) || (huart2.State == HAL_UART_STATE_BUSY_RX))
	{
		tracereport.head = 0xff;
		
		static uint16_t cnt = 0;
		cnt++;
//		if(cnt%2)
//		{
			for(uint16_t rowcnt = 0;rowcnt < IMG_HEIGHT/2;rowcnt++)
			{
				uint16_t colcnt = 0;
				for(;colcnt <= trace_result[rowcnt*2].left/2;colcnt++)
					tracereport.buff[rowcnt][colcnt] = 0xcf;
				for(;colcnt < trace_result[rowcnt*2].right/2;colcnt++)
					tracereport.buff[rowcnt][colcnt] = 0x30;
				for(;colcnt < IMG_WIDTH/2;colcnt++)
					tracereport.buff[rowcnt][colcnt] = 0xcf;
			}
//		}
//		else
//		{
//			
//			for(uint16_t ii=0;ii<IMG_HEIGHT/2;ii++)
//				for(uint16_t jj=0;jj<IMG_WIDTH/2;jj++)
//					tracereport.buff[ii][jj] = 0xfe*(ImgData[ii*2][jj*2] >=gatevalue);
//		}
		
		HAL_UART_Transmit_DMA(&huart2,(uint8_t*)&tracereport,sizeof(tracereport));
	}
	return 0;
}


#define LINE_DIFF_VER_MAX (IMG_WIDTH/32)//�߽���ͻ�����ֵ


static uint8_t CalTrack(void)
{
//	int16_t shapeL;
//	int16_t shapeR;
	uint8_t ifshape= 0;//����ͻ��״̬��0��ʾʲô��û�У�1��ʾ�������ͻ��2��ʾ�����ҵ�ͻ��3��ʾ����ͻ����
	uint8_t ifboundary = 0;//�����Ƿ񵽴�߽磬0��ʾû�е���߽磬1��ʾ�е������ı߽磬2��ʾ�е����Ҳ�ı߽磬3��ʾ���඼�е���߽�
	if((trace_result[IMG_HEIGHT-1].left == 0)&&(trace_result[IMG_HEIGHT-1].right == 0))
		return LINE_LOST_ERROR;
	int16_t ii;
	for(ii = IMG_HEIGHT-3;ii>=0;ii--)
	{
		if((trace_result[ii].left == 0)&&(trace_result[ii].right == 0))//�Ѿ�ʧȥ
		{
			break;
		}
		if(trace_result[ii].left <= IMG_WIDTH/50)//�е������߽�
		{
			if(ifboundary == 2)
				ifboundary = 3;
			else if(ifboundary == 3)
				ifboundary = 3;
			else//(ifboundary == 0��1)
				ifboundary = 1;
		}
		if(trace_result[ii].right >= (IMG_WIDTH-1-IMG_WIDTH/50))//�е����Ҳ�߽�
		{
			if(ifboundary == 1)
				ifboundary = 3;
			else if(ifboundary == 3)
				ifboundary = 3;
			else//(ifboundary == 0��2)
				ifboundary = 2;
		}
//		if(ABS(trace_result[ii].left - trace_result[ii+2].left) > LINE_DIFF_VER_MAX)//�����ͻ��
		if((trace_result[ii+2].left - trace_result[ii].left) > LINE_DIFF_VER_MAX)//�����ͻ����������
		{
			if(ifshape == 2)
				ifshape = 3;
			else if(ifshape == 3)
				ifshape = 3;
			else//(ifshape == 0��1)
				ifshape = 1;
		}
//		if(ABS(trace_result[ii].right - trace_result[ii+2].right) > LINE_DIFF_VER_MAX)//�Ҳ���ͻ��
		if(trace_result[ii].right - trace_result[ii+2].right > LINE_DIFF_VER_MAX)//�Ҳ���ͻ����������

		{
			if(ifshape == 1)
				ifshape = 3;
			else if(ifshape == 3)
				ifshape = 3;
			else//(ifshape == 0��2)
				ifshape = 2;
		}
	}

	
	if(ifstarightline == 1)
		if((ifshape == 2)||(ifboundary == 2))
			return LINE_MARK;
		else
			return LINE_STRAIGHT;
	else if((ii > IMG_HEIGHT/100)&&((ifshape == 3)||(ifboundary == 3)))
		return LINE_END;
	if((ii > IMG_HEIGHT/6)&&((ifshape == 2)||(ifboundary == 2)))
		return LINE_TURN_RIGHT_90;
	if((ii > IMG_HEIGHT/6)&&((ifshape == 1)||(ifboundary == 1)))
		return LINE_TURN_LEFT_90;
	return LINE_STRAIGHT;

}



static int16_t CalMiddleError(uint8_t state)
{
	uint16_t validrowcnt = 0;
	uint16_t sumofrow = 0;
	for(int16_t ii = IMG_HEIGHT-1;ii>=0;ii--)
	{
		if((trace_result[ii].left == 0)&&(trace_result[ii].right == 0))
			break;
		else
		{
			validrowcnt++;
			sumofrow += ((uint16_t)trace_result[ii].left+trace_result[ii].right)/2;
		}
	}
	static int16_t lastreturn = IMG_WIDTH/2;
	if(validrowcnt == 0)//û��
		return lastreturn;
	else
	{
		lastreturn = (int16_t)(sumofrow/validrowcnt) - IMG_WIDTH/2;
		return lastreturn;
	}
}




#include <arm_math.h>
#include <math.h>
#define VALID_LINE_VER_NUM 4//������Ч���ݲ���
static float CalAngleError(uint8_t state)
{
	if(state == LINE_END)
		return 0;
	if(state == LINE_LOST_ERROR)
		return 0;
	if(state == LINE_TURN_LEFT_90)
		return -90;
	if(state == LINE_TURN_RIGHT_90)
		return 90;
	
	
	if(ifstarightline == 1)
	{
		int16_t deltax ;
		int16_t deltay ;
		if(trace_result[0].right -  trace_result[0].left < IMG_HEIGHT/3)
		{
			deltax = (top-bottom);
			deltay = IMG_HEIGHT;
		}
		else if(trace_result[IMG_HEIGHT/2].right -  trace_result[IMG_HEIGHT/2].left < IMG_HEIGHT/3)
		{
			deltax = ((trace_result[IMG_HEIGHT/2].right +  trace_result[IMG_HEIGHT/2].left)/2-bottom);
			deltay = IMG_HEIGHT/2;
		}
		else
			return 0;
		
		float temp = atan2f(deltay,deltax);
		return 90-temp*180/(PI); 
	}
	else
		return 0;

}


#define LINE_WIDTH_MIN 3
#define LINE_WIDTH_MAX 30


//����ֵ>=0,�ҵ���λ��
//����ֵ�Ǳ߽�ʱ����û���ҵ�
uint16_t find_0_L(uint8_t* LineData,int16_t startpoint);
uint16_t find_0_R(uint8_t* LineData,int16_t startpoint);
uint16_t find_1_L(uint8_t* LineData,int16_t startpoint);
uint16_t find_1_R(uint8_t* LineData,int16_t startpoint);
uint16_t find_close_1(uint8_t* LineData,uint16_t startpoint);


#define MIDDLE_AVE_FILTER 3//�����˲�����ֱֹ��λ��ͻ��
#define MAX_VERTICAL_BOUNDARY (IMG_WIDTH/10)



int16_t lastframemiddlefilter[MIDDLE_AVE_FILTER];
uint32_t framemiddlecnt = 0;

//#define STARTLINE_CHECK_LINE (IMG_HEIGHT/10)


uint16_t shadow_v[IMG_WIDTH];//��ֱ����ͶӰ

static uint8_t CameraGetTrack(uint8_t ImgData[][IMG_WIDTH])//��ȡ����
{
	
	if(framemiddlecnt == 0)
	{
		for(uint32_t ii = 0;ii < MIDDLE_AVE_FILTER;ii++)
		{
			lastframemiddlefilter[ii] = IMG_WIDTH/2;
		}
	}
	
	

	for(uint16_t ii = 0; ii< IMG_HEIGHT;ii++)
	{
		trace_result[ii].left = 0;
		trace_result[ii].right = 0;
	}
	//��ͶӰ
	for(uint16_t ii = 0; ii< IMG_WIDTH;ii++)
		shadow_v[ii]=0;
	for(uint16_t ii = 0;ii < IMG_HEIGHT; ii ++)
		for(uint16_t jj = 0;jj < IMG_WIDTH; jj ++)
		{
			if(ImgData[ii][jj] < gatevalue)
				shadow_v[jj]++;
		}
	uint16_t lastmiddle = 0xffff;
	static int16_t lastframemiddle = IMG_WIDTH/2;

		
	
		

	for(int16_t ii=0;ii < IMG_WIDTH;ii++)
	{
		int16_t thisx = lastframemiddle-ii-1;
		if((thisx >= 0)&&(shadow_v[thisx] >= LINE_WIDTH_MIN)&&(ImgData[IMG_HEIGHT-1][thisx] < gatevalue))
		{
			lastmiddle = thisx;
			break;
		}
		thisx = lastframemiddle+ii;
		if((thisx < IMG_WIDTH)&&(shadow_v[thisx] >= LINE_WIDTH_MIN)&&(ImgData[IMG_HEIGHT-1][thisx] < gatevalue))
		{
			lastmiddle = thisx;
			break;
		}
	}
		

	
	if(lastmiddle == 0xffff)//û�ҵ��ߣ�����
		return 	0;
	
//�˴����Կ��Ǵ��Ķ���ȥ���Ķ�����
////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////	

	
	//lastmiddle��ͨ�˲�
	int16_t firstl,firstr,templastmiddle;
	firstl = find_0_L(ImgData[IMG_HEIGHT-1],lastmiddle);
	firstr = find_0_R(ImgData[IMG_HEIGHT-1],lastmiddle);

	if(firstr - firstl < IMG_WIDTH/10)
	{
		lastframemiddlefilter[framemiddlecnt%MIDDLE_AVE_FILTER] = lastmiddle;
	}
	framemiddlecnt ++;
	uint32_t sumoflastmiddle = 0;
	for(uint8_t ii =0;ii < MIDDLE_AVE_FILTER;ii++)
		sumoflastmiddle  += lastframemiddlefilter[ii];
	
	templastmiddle = sumoflastmiddle/MIDDLE_AVE_FILTER;	

	if(ImgData[IMG_HEIGHT-1][templastmiddle] < gatevalue)
		lastmiddle = templastmiddle;
	
	
	lastframemiddle = lastmiddle;
	
			//ֱ������
	float k;
	ifstarightline = 0;
	
	for(uint16_t colcnt = 0;colcnt < IMG_WIDTH-1;colcnt++)
	{
		if((ImgData[0][colcnt] < gatevalue)&&(ImgData[0][colcnt+1] < gatevalue))
		{
			top = colcnt+1;
			bottom = lastmiddle;
			k = IMG_HEIGHT;
			k = (top - bottom)/k;
			int16_t count;
			for(count = 0;count<IMG_HEIGHT;count++)
			{
				if(ImgData[count][(uint16_t)((top-(count)*k)+0.5f)] >= gatevalue)
					break;
			}
			if(count >= IMG_HEIGHT)//�ҵ���
			{
				ifstarightline = 1;
				break;
			}
		}
		if(ifstarightline == 1)
			break;
	}
	
	if(ifstarightline == 1)//�ҵ�������ֱ��
	{
		for(int16_t rowcnt = IMG_HEIGHT-1;rowcnt >=0;rowcnt--)
		{
			trace_result[rowcnt].left = find_0_L(ImgData[rowcnt],(uint16_t)((top-(rowcnt)*k)+0.5f));
			trace_result[rowcnt].right = find_0_R(ImgData[rowcnt],(uint16_t)((top-(rowcnt)*k)+0.5f));
		}
		top = (trace_result[0].left + trace_result[0].right)/2;
		return 0;
	}
	
	
//end ֱ������
	
 

	

	
	uint16_t middle_filter[MIDDLE_AVE_FILTER];
	for(uint16_t iii=0;iii <MIDDLE_AVE_FILTER;iii++)
	{
		middle_filter[iii] = lastmiddle;
	}
	//��ʼ����
	for(int16_t rowcnt = IMG_HEIGHT-1;rowcnt >=0;rowcnt--)
	{
		uint16_t templ,tempr;
		if(ImgData[rowcnt][lastmiddle] >= gatevalue)
		{
			uint16_t temp_1;
			temp_1 = findnearestest_1(ImgData[rowcnt],lastmiddle);
			if(temp_1 == 0xffff)
				return 0;
			if(rowcnt == IMG_HEIGHT-1)
				return 0;
			if((temp_1 >=  trace_result[rowcnt+1].left)&&(temp_1 <=  trace_result[rowcnt+1].right))
				lastmiddle = temp_1;
			else
				return 0;
		}
		templ = find_0_L(ImgData[rowcnt],lastmiddle);
		tempr = find_0_R(ImgData[rowcnt],lastmiddle);
		if((templ < IMG_WIDTH)&&(tempr < IMG_WIDTH))
		{
			trace_result[rowcnt].left = templ;
			trace_result[rowcnt].right = tempr;
		}else
			return 1;//����
		lastmiddle = ((uint16_t)templ+(uint16_t)tempr)/2;
		if((rowcnt < IMG_HEIGHT-1)&&(ABS(templ - trace_result[rowcnt+1].left) <= LINE_DIFF_VER_MAX)&&(ABS(tempr - trace_result[rowcnt+1].right) <= LINE_DIFF_VER_MAX))
		{
			static uint16_t filtercnt = 0;
			middle_filter[filtercnt%MIDDLE_AVE_FILTER] = lastmiddle;
			uint32_t sum = 0;
			for(uint16_t iii=0;iii <MIDDLE_AVE_FILTER;iii++)
			{
				sum += middle_filter[iii];
			}
			lastmiddle = sum/MIDDLE_AVE_FILTER;
			filtercnt ++ ;
		}
	}
	return 0;
}


#define DIFFGATE 40



uint16_t find_0_L(uint8_t* LineData,int16_t startpoint)
{
	if(startpoint > IMG_WIDTH-1-1)
		startpoint = IMG_WIDTH-1-1;
	if(startpoint < 0+1)
		startpoint = 1;
	while(startpoint >=0)
	{
		int16_t pixL,pixR;
		pixL = LineData[startpoint-1];
		pixR = LineData[startpoint+1];
//		if((pixL - pixR > DIFFGATE))
	if((pixR >= gatevalue)&&(pixL >= gatevalue))//���ǰ�
			return (startpoint);
		startpoint --;
	}
	return 0;
}


static uint16_t findnearestest_1(uint8_t* LineData,int16_t startpoint)//if didn't find 1,reuturn 0xffff
{
	for(int16_t ii=0;ii < IMG_WIDTH;ii++)
	{
		int16_t thisx = startpoint-ii-1;
		if((thisx >= 1)&&(LineData[thisx] < gatevalue)&&(LineData[thisx-1] < gatevalue))
		{
			return  thisx;
		}
		thisx = startpoint+ii;
		if((thisx < IMG_WIDTH-1)&&(LineData[thisx] < gatevalue)&&(LineData[thisx+1] < gatevalue))
		{
			return  thisx;
		}
	}
	return 0xffff;
}


uint16_t find_0_R(uint8_t* LineData,int16_t startpoint)
{
	if(startpoint > IMG_WIDTH-1-1)
		startpoint = IMG_WIDTH-1-1;
	if(startpoint < 1)
		startpoint = 1;
	while(startpoint < IMG_WIDTH-1)
	{
		int16_t pixL,pixR;
		pixL = LineData[startpoint-1];
		pixR = LineData[startpoint+1];
//		if((pixR - pixL > DIFFGATE))
		if((pixR >= gatevalue)&&(pixL >= gatevalue))//���ǰ�
			return (startpoint);
		startpoint ++;
	}
	return IMG_WIDTH-1;
}

