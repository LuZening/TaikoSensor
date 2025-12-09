/*
 * mouse.h
 *
 *  Created on: Jun 27, 2023
 *      Author: cpholzn
 */

#ifndef INC_MOUSE_H_
#define INC_MOUSE_H_


typedef struct
{
	char mouse_abs_left : 1;	//鼠标左键单击
	char mouse_abs_right : 1;	//鼠标右键单击
	char mouse_abs_wheel : 1;	//鼠标中键单击
	char reserve : 5;			//常量0
	char mouse_rel_x;			//鼠标X轴移动偏移量
	char mouse_rel_y;			//鼠标Y轴移动偏移量
	char mouse_rel_wheel;		//鼠标滚轮移动偏移量
}	Mouse_data_t;



#endif /* INC_MOUSE_H_ */
