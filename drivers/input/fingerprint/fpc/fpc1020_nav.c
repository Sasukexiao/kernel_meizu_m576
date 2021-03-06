/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

//#define DEBUG

#include <linux/input.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <linux/wakelock.h>

#ifndef CONFIG_OF
#include <linux/spi/fpc1020_common.h>
#include <linux/spi/fpc1020_input.h>
#include <linux/spi/fpc1020_capture.h>
#else
#include "fpc1020_common.h"
#include "fpc1020_input.h"
#include "fpc1020_capture.h"
#include "fpc1020_navlib.h"
#endif


/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#ifndef AS_HOME_KEY
static int fpc1020_write_nav_setup(fpc1020_data_t *fpc1020);

//static int fpc1020_wait_finger_present_lpm(fpc1020_data_t *fpc1020);

static int capture_nav_image(fpc1020_data_t *fpc1020);
#endif

#endif


/* -------------------------------------------------------------------- */
/* driver constants							*/
/* -------------------------------------------------------------------- */
#define FPC1020_KEY_FINGER_PRESENT	KEY_F18	/* 188*/

#ifdef VENDOR_EDIT
//Lycan.Wang@Prd.BasicDrv, 2014-09-12 Add for report touch down or up
#define FPC1020_KEY_FINGER_TOUCH			KEY_BACK
#define FPC1020_KEY_FINGER_DTP			        KEY_MENU
#define FPC1020_KEY_MOVE_FORWARD   			KEY_F20
#define FPC1020_KEY_MOVE_BACKWARD  			KEY_F21
#define FPC1020_KEY_ROTATE_FORWARD			KEY_F22
#define FPC1020_KEY_ROTATE_BACKWARD			KEY_F23
#define DTP_INTERVAL_IN_MS 18

#include <linux/timer.h>

struct timer_list s_timer;

#ifdef AS_HOME_KEY
#define FPC1020_KEY_FINGER_PRESS	KEY_HOME	/* 102*/
#define FPC1020_KEY_FINGER_DOUBLE_TAB	KEY_F18	/* 188*/
#define FPC1020_KEY_FINGER_LONG_PRESS	KEY_F19	/* 189*/
#endif

#endif /* VENDOR_EDIT */

#define FPC1020_INPUT_POLL_TIME_MS	1000u

#define FPC1020_HW_DETECT_MASK 0x6f6

#define FPC1020_INPUT_POLL_INTERVAL 5000
#define FLOAT_MAX 100

#define DEVICE_WIDTH 720
#define DEVICE_HEIGHT 1280

#define IMAGE_PADDING 		24
#define BEST_IMAGE_WIDTH	4
#define BEST_IMAGE_HEIGHT	6

enum {
	FNGR_ST_NONE = 0,
	FNGR_ST_DETECTED,
	FNGR_ST_LOST,
	FNGR_ST_TAP,
	FNGR_ST_HOLD,
	FNGR_ST_MOVING,
	FNGR_ST_L_HOLD,
	FNGR_ST_DOUBLE_TAP,
};

enum {
	FPC1020_INPUTMODE_TRACKPAD	= 0,	//trackpad(navi) event report
	FPC1020_INPUTMODE_MOUSE		= 1,	//mouse event report
	FPC1020_INPUTMODE_TOUCH		= 2,	//touch event report
	FPC1020_INPUTMODE_MOVE		= 3,	//move event report
};

/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#ifndef AS_HOME_KEY
/* -------------------------------------------------------------------- */
void init_enhanced_navi_setting(fpc1020_data_t *fpc1020)
{
	dev_info(&fpc1020->spi->dev, "%s\n", __func__);
	switch(fpc1020->nav.input_mode) {
		case FPC1020_INPUTMODE_TRACKPAD:
			fpc1020->nav.p_sensitivity_key = 25;
			fpc1020->nav.p_sensitivity_ptr = 180;
			fpc1020->nav.p_multiplier_x = 75;
			fpc1020->nav.p_multiplier_y = 95;
			fpc1020->nav.multiplier_key_accel = 2;
			fpc1020->nav.multiplier_ptr_accel = 2;
			fpc1020->nav.threshold_key_accel = 70;
			fpc1020->nav.threshold_ptr_accel = 10;
			fpc1020->nav.threshold_ptr_start = 5;
			fpc1020->nav.duration_ptr_clear = 100;
			fpc1020->nav.nav_finger_up_threshold = 3;
			break;
/*		case BTP_INPUTMODE_MOUSE:
			fpc1020->nav.p_sensitivity_key = 180;
			fpc1020->nav.p_sensitivity_ptr = 170;//180;
			fpc1020->nav.p_multiplier_x = 150;//110;
			fpc1020->nav.p_multiplier_y = 100;//110;
			fpc1020->nav.multiplier_key_accel = 1;
			fpc1020->nav.multiplier_ptr_accel = 2;
			fpc1020->nav.threshold_key_accel = 40;
			fpc1020->nav.threshold_ptr_accel = 30;//20;
			fpc1020->nav.threshold_ptr_start = 2;//5;
			fpc1020->nav.duration_ptr_clear = 100;
            fpc1020->nav.nav_finger_up_threshold = 3;
			break;*/
		case FPC1020_INPUTMODE_TOUCH:
			fpc1020->nav.p_sensitivity_key = 200;//180;
			fpc1020->nav.p_sensitivity_ptr = 180;
			fpc1020->nav.p_multiplier_x = 255;//110;
			fpc1020->nav.p_multiplier_y = 110;
			fpc1020->nav.multiplier_key_accel = 1;
			fpc1020->nav.multiplier_ptr_accel = 2;
			fpc1020->nav.threshold_key_accel = 40;
			fpc1020->nav.threshold_ptr_accel = 20;
			fpc1020->nav.threshold_ptr_start = 5;
			fpc1020->nav.duration_ptr_clear = 100;
			fpc1020->nav.nav_finger_up_threshold = 3;
			break;
#ifdef VENDOR_EDIT
//Lycan.Wang@Prd.BasicDrv, 2014-09-29 Add for navigation move event
		case FPC1020_INPUTMODE_MOVE:
			fpc1020->nav.p_sensitivity_key = 200;//180;
			fpc1020->nav.p_sensitivity_ptr = 180;
			fpc1020->nav.p_multiplier_x = 255;//110;
			fpc1020->nav.p_multiplier_y = 110;
			fpc1020->nav.multiplier_key_accel = 1;
			fpc1020->nav.multiplier_ptr_accel = 2;
			fpc1020->nav.threshold_key_accel = 40;
			fpc1020->nav.threshold_ptr_accel = 20;
			fpc1020->nav.threshold_ptr_start = 5;
			fpc1020->nav.duration_ptr_clear = 100;
			fpc1020->nav.nav_finger_up_threshold = 3;
			fpc1020->nav.move_time_threshold = 350;
			fpc1020->nav.move_distance_threshold = 450;
			break;
#endif /* VENDOR_EDIT */
		default:
			break;
	}
}


/* -------------------------------------------------------------------- */
static void dispatch_trackpad_event(fpc1020_data_t *fpc1020, int x, int y, int finger_status)
{
	int abs_x, abs_y;
	int sign_x, sign_y;

	if (finger_status == FNGR_ST_TAP) {
		input_report_key(fpc1020->input_dev, KEY_ENTER, 1);
		input_sync(fpc1020->input_dev);
		input_report_key(fpc1020->input_dev, KEY_ENTER, 0);
		input_sync(fpc1020->input_dev);
		return;
	}

	sign_x = x > 0 ? 1 : -1;
	sign_y = y > 0 ? 1 : -1;
	abs_x = x * sign_x;
	abs_y = y * sign_y;

	abs_x = x > 0 ? x : -x;
	abs_y = y > 0 ? y : -y;

	if (abs_x > fpc1020->nav.threshold_key_accel)
		x = ( fpc1020->nav.threshold_key_accel + ( abs_x - fpc1020->nav.threshold_key_accel ) * fpc1020->nav.multiplier_key_accel ) * sign_x;
	if (abs_y > fpc1020->nav.threshold_key_accel)
		y = ( fpc1020->nav.threshold_key_accel + ( abs_y - fpc1020->nav.threshold_key_accel ) * fpc1020->nav.multiplier_key_accel ) * sign_y;

	// Correct axis factor
	x = x * fpc1020->nav.p_multiplier_x / FLOAT_MAX;
	y = y * fpc1020->nav.p_multiplier_y / FLOAT_MAX;

	// Adjust Sensitivity
	x = x * fpc1020->nav.p_sensitivity_key / FLOAT_MAX;
	y = y * fpc1020->nav.p_sensitivity_key / FLOAT_MAX;

	input_report_rel(fpc1020->input_dev, REL_X, x);
	input_report_rel(fpc1020->input_dev, REL_Y, y);

	input_sync(fpc1020->input_dev);
}


/* -------------------------------------------------------------------- */
static void dispatch_touch_event(fpc1020_data_t *fpc1020, int x, int y, int finger_status)
{
	int sign_x, sign_y;
	int abs_x, abs_y;

	switch(finger_status) {
		case FNGR_ST_DETECTED:
			fpc1020->nav.nav_sum_x = 360;
			fpc1020->nav.nav_sum_y = 640;
			input_report_abs(fpc1020->touch_pad_dev, ABS_X, fpc1020->nav.nav_sum_x);
			input_report_abs(fpc1020->touch_pad_dev, ABS_Y, fpc1020->nav.nav_sum_y);
			input_report_abs(fpc1020->touch_pad_dev, ABS_Z, 0);
			input_report_key(fpc1020->touch_pad_dev, BTN_TOUCH, 1);
			input_sync(fpc1020->touch_pad_dev);
			break;

		case FNGR_ST_LOST:
		//	case FNGR_ST_TAP:
			input_report_abs(fpc1020->touch_pad_dev, ABS_X, fpc1020->nav.nav_sum_x);
			input_report_abs(fpc1020->touch_pad_dev, ABS_Y, fpc1020->nav.nav_sum_y);
			input_report_abs(fpc1020->touch_pad_dev, ABS_Z, 0);
			input_report_key(fpc1020->touch_pad_dev, BTN_TOUCH, 0);
			input_sync(fpc1020->touch_pad_dev);
			break;

		case FNGR_ST_MOVING:
			sign_x = x > 0 ? 1 : -1;
			sign_y = y > 0 ? 1 : -1; //reverse direction
			abs_x = x > 0 ? x : -x;
			abs_y = y > 0 ? y : -y;

			if (abs_x > fpc1020->nav.threshold_key_accel)
				x = ( fpc1020->nav.threshold_key_accel + ( abs_x - fpc1020->nav.threshold_key_accel ) * fpc1020->nav.multiplier_key_accel ) * sign_x;
			if (abs_y > fpc1020->nav.threshold_key_accel)
				y = ( fpc1020->nav.threshold_key_accel + ( abs_y - fpc1020->nav.threshold_key_accel ) * fpc1020->nav.multiplier_key_accel ) * sign_y;

			x = x * fpc1020->nav.p_multiplier_x / FLOAT_MAX;
			y = y * fpc1020->nav.p_multiplier_y / FLOAT_MAX;

			x = x * fpc1020->nav.p_sensitivity_key / FLOAT_MAX;
			y = y * fpc1020->nav.p_sensitivity_key / FLOAT_MAX;

			if(x != 0 || y != 0) {
				if(abs(x) > abs(y)) {
					int newX = fpc1020->nav.nav_sum_x;
					//newX += x;
					if(newX < 0) {
						newX = 0;
					} else if(newX >= DEVICE_WIDTH) {
						newX = DEVICE_WIDTH -1;
					}

					if(newX != fpc1020->nav.nav_sum_x) {
						//fpc1020->nav.nav_sum_x = newX;
						input_report_abs(fpc1020->touch_pad_dev, ABS_X, fpc1020->nav.nav_sum_x);
						input_report_abs(fpc1020->touch_pad_dev, ABS_Y, fpc1020->nav.nav_sum_y);
						input_report_abs(fpc1020->touch_pad_dev, ABS_Z, 0);
						input_report_key(fpc1020->touch_pad_dev, BTN_TOUCH, 1);
						input_sync(fpc1020->touch_pad_dev);
					}
				} else {
					int newY = fpc1020->nav.nav_sum_y;
					newY -= y;
					if(newY < 0) {
						newY = 0;
					} else if(newY >= DEVICE_HEIGHT) {
						newY = DEVICE_HEIGHT -1;
					}

					if(newY != fpc1020->nav.nav_sum_y) {
						fpc1020->nav.nav_sum_y = newY;

						input_report_abs(fpc1020->touch_pad_dev, ABS_X, fpc1020->nav.nav_sum_x);
						input_report_abs(fpc1020->touch_pad_dev, ABS_Y, fpc1020->nav.nav_sum_y);
						input_report_abs(fpc1020->touch_pad_dev, ABS_Z, 0);
						input_report_key(fpc1020->touch_pad_dev, BTN_TOUCH, 1);
						input_sync(fpc1020->touch_pad_dev);
					}
				}
			}
			break;

		default:
			break;
	}
}

#ifdef VENDOR_EDIT

static void fpc1020_timer_handle(unsigned long arg)
{
        fpc1020_data_t *fpc1020 = (fpc1020_data_t *)arg;

        del_timer(&s_timer);

        if(fpc1020->nav.tap_status == FNGR_ST_TAP){
	    input_report_key(fpc1020->input_dev,
	    		FPC1020_KEY_FINGER_TOUCH, 1);
            input_report_key(fpc1020->input_dev,
			FPC1020_KEY_FINGER_TOUCH, 0);
	    input_sync(fpc1020->input_dev);
	    fpc1020->nav.tap_status = FNGR_ST_LOST;
            pr_info("finger timer single click\n");
        }

}

//Lycan.Wang@Prd.BasicDrv, 2014-09-29 Add for navigation move event
/* -------------------------------------------------------------------- */
static void dispatch_move_event(fpc1020_data_t *fpc1020, int x, int y, int finger_status)
{
	int sign_y;
	int abs_y;
	int newY;

	switch(finger_status) {
		case FNGR_ST_DETECTED:
			fpc1020->nav.nav_sum_y = 640;
			fpc1020->touch_time = jiffies;
			fpc1020->move_distance = 0;
			fpc1020->moving_key = 0;
			break;

		case FNGR_ST_LOST:
			printk("lycan test %d time %dms\n", fpc1020->move_distance, jiffies_to_msecs(jiffies - fpc1020->touch_time));
			if (fpc1020->moving_key) {
				input_report_key(fpc1020->input_dev, fpc1020->moving_key, 0);
				input_sync(fpc1020->input_dev);
			} else if (abs(fpc1020->move_distance) > fpc1020->nav.move_distance_threshold && 
					jiffies_to_msecs(jiffies - fpc1020->touch_time) < fpc1020->nav.move_time_threshold) {
				if (fpc1020->move_distance > 0) {
					input_report_key(fpc1020->input_dev, FPC1020_KEY_ROTATE_FORWARD, 1);
					input_sync(fpc1020->input_dev);
					input_report_key(fpc1020->input_dev, FPC1020_KEY_ROTATE_FORWARD, 0);
					input_sync(fpc1020->input_dev);
				} else {
					input_report_key(fpc1020->input_dev, FPC1020_KEY_ROTATE_BACKWARD, 1);
					input_sync(fpc1020->input_dev);
					input_report_key(fpc1020->input_dev, FPC1020_KEY_ROTATE_BACKWARD, 0);
					input_sync(fpc1020->input_dev);
				}
			} 
			break;

		case FNGR_ST_MOVING:
			sign_y = y > 0 ? 1 : -1; //reverse direction
			abs_y = y > 0 ? y : -y;

			if (abs_y > fpc1020->nav.threshold_key_accel)
				y = ( fpc1020->nav.threshold_key_accel + ( abs_y - fpc1020->nav.threshold_key_accel ) * fpc1020->nav.multiplier_key_accel ) * sign_y;

			y = y * fpc1020->nav.p_multiplier_y / FLOAT_MAX;
			y = y * fpc1020->nav.p_sensitivity_key / FLOAT_MAX;

			newY = fpc1020->nav.nav_sum_y - y;

			if(newY < 0) {
				newY = 0;
			} else if(newY >= DEVICE_HEIGHT) {
				newY = DEVICE_HEIGHT -1;
			}

			fpc1020->nav.nav_sum_y = newY;
			if (jiffies_to_msecs(jiffies - fpc1020->touch_time) > fpc1020->nav.move_time_threshold) {
				if (abs(fpc1020->move_distance) < abs(newY - 640))
					fpc1020->move_distance = newY - 640;

				if (!fpc1020->moving_key) {
					if (abs(fpc1020->move_distance) > fpc1020->nav.move_distance_threshold) {
						if (fpc1020->move_distance > 0) {
							fpc1020->moving_key = FPC1020_KEY_MOVE_FORWARD;
						} else {
							fpc1020->moving_key = FPC1020_KEY_MOVE_BACKWARD;
						}
						input_report_key(fpc1020->input_dev, fpc1020->moving_key, 1);
						input_sync(fpc1020->input_dev);
					}
				}
			}
			break;

                case FNGR_ST_DOUBLE_TAP:
			input_report_key(fpc1020->input_dev, FPC1020_KEY_FINGER_DTP, 1);
			input_sync(fpc1020->input_dev);
			input_report_key(fpc1020->input_dev, FPC1020_KEY_FINGER_DTP, 0);
			input_sync(fpc1020->input_dev);
                        break;

                case FNGR_ST_HOLD:
			input_report_key(fpc1020->input_dev, FPC1020_KEY_FINGER_TOUCH, 1);
			input_sync(fpc1020->input_dev);
                        pr_info("[FPC/DPAD] report long press\n");
                        break;

		default:
			break;
	}
}
#endif /* VENDOR_EDIT */

/* -------------------------------------------------------------------- */
static void process_navi_event(fpc1020_data_t *fpc1020, int dx, int dy, int finger_status)
{
	const int THRESHOLD_RANGE_TAP = 500000;
        const int THRESHOLD_RANGE_MIN_TAP = 60000;
	//const unsigned long THRESHOLD_DURATION_TAP = 3000;//350;
	const unsigned long THRESHOLD_DURATION_TAP = 1000;/*long press threshold*/
	const unsigned long THRESHOLD_DURATION_DTAP = 850;
	int filtered_finger_status = finger_status;
	static int deviation_x = 0;
	static int deviation_y = 0;
	int deviation;
	static unsigned long tick_down = 0;
	unsigned long tick_curr = jiffies * 1000 / HZ;
	unsigned long duration = 0;

	if ( finger_status == FNGR_ST_DETECTED ) {
		tick_down = tick_curr;
	}

	if ( tick_down > 0 ) {
		duration = tick_curr - tick_down;
		deviation_x += dx;
		deviation_y += dy;
		deviation =  deviation_x * deviation_x + deviation_y * deviation_y;

		if ( deviation > THRESHOLD_RANGE_TAP ) {
			deviation_x = 0;
			deviation_y = 0;
			tick_down = 0;
			fpc1020->nav.tap_status = -1;
			printk("[FPC] %s:throw the events\n", __func__);
			if (duration > THRESHOLD_DURATION_TAP)
                        {
                            printk("[FPC] %s: prepare long press because of outside\n", __func__);
                            filtered_finger_status = FNGR_ST_HOLD;// FNGR_ST_L_HOLD;
                        }
		}
		else 
                {
	            if (duration < THRESHOLD_DURATION_TAP) {
                        if (finger_status == FNGR_ST_LOST && fpc1020->nav.detect_zones != 0) 
                        {
                            if (fpc1020->nav.tap_status == FNGR_ST_TAP && tick_curr - fpc1020->nav.tap_start < THRESHOLD_DURATION_DTAP) //&& deviation == 0)
                            {
                                fpc1020->nav.tap_status = FNGR_ST_DOUBLE_TAP;
                                filtered_finger_status = FNGR_ST_DOUBLE_TAP;
                                fpc1020->nav.detect_zones = 0;

                                if(timer_pending(&s_timer))
                                    del_timer(&s_timer);

                                pr_info("[FPC] %s:prepare report double click\n", __func__);
                            }
                            else if (deviation <= THRESHOLD_RANGE_MIN_TAP)
                            {
                                filtered_finger_status = FNGR_ST_TAP;
                                fpc1020->nav.tap_status = FNGR_ST_TAP;
                                fpc1020->nav.tap_start = tick_curr;

                                if(!timer_pending(&s_timer)){
                                    init_timer(&s_timer);
                                    setup_timer(&s_timer, &fpc1020_timer_handle,(unsigned long)fpc1020);
                                    mod_timer(&s_timer, jiffies + DTP_INTERVAL_IN_MS);
                                }
                                pr_info("[FPC] %s:prepare report single click\n", __func__);
                            }
                            else
                            {
                                pr_info("[FPC] %s: still finger lost. deviation: %d\n", __func__, deviation);
                                filtered_finger_status = FNGR_ST_LOST;
                                fpc1020->nav.filter_long = 0;
                            }

                            tick_down = 0;
                            deviation_x = 0;
                            deviation_y = 0;
                        }
                    }
                    else
                    {
                        printk("[FPC] %s: prepare long press\n", __func__);
                        //if (deviation < THRESHOLD_RANGE_MIN_TAP)
                        filtered_finger_status = FNGR_ST_HOLD;// FNGR_ST_L_HOLD;
                        fpc1020->nav.tap_status = -1;
                        tick_down = 0;
                        deviation_x = 0;
                        deviation_y = 0;
                    }
	        }

       }
	dev_info(&fpc1020->spi->dev, "[INFO] mode[%d] dx : %d / dy : %d\n", 1, dx, dy);

	switch(fpc1020->nav.input_mode) {
		case FPC1020_INPUTMODE_TRACKPAD :
			dispatch_trackpad_event(fpc1020, -dx, dy, filtered_finger_status);
			break;
//		case BTP_INPUTMODE_MOUSE:
//			dispatch_mouse_event( fpc_btp, -dx, dy, filtered_finger_status );
//			break;
		case FPC1020_INPUTMODE_TOUCH:
			dispatch_touch_event(fpc1020, dx, dy, filtered_finger_status);
			break;
#ifdef VENDOR_EDIT
//Lycan.Wang@Prd.BasicDrv, 2014-09-29 Add for navigation move event
		case FPC1020_INPUTMODE_MOVE:
			dispatch_move_event(fpc1020, dx, dy, filtered_finger_status);
			break;
#endif /* VENDOR_EDIT */
		default:
			break;
	}
}
#endif
/* -------------------------------------------------------------------- */
int fpc1020_input_init(fpc1020_data_t *fpc1020)
{
	int error = 0;

	if ((fpc1020->chip.type != FPC1020_CHIP_1020A)
			&& (fpc1020->chip.type != FPC1020_CHIP_1021A)
			&& (fpc1020->chip.type != FPC1020_CHIP_1021B)
			&& (fpc1020->chip.type != FPC1020_CHIP_1150B)
			&& (fpc1020->chip.type != FPC1020_CHIP_1150F)
			&& (fpc1020->chip.type != FPC1020_CHIP_1155X)){
		dev_err(&fpc1020->spi->dev, "%s, chip not supported (%s)\n",
			__func__,
			fpc1020_hw_id_text(fpc1020));

		return -EINVAL;
	}

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->input_dev = input_allocate_device();

	if (!fpc1020->input_dev) {
		dev_err(&fpc1020->spi->dev, "Input_allocate_device failed.\n");
		error  = -ENOMEM;
	}

	if (!error) {
		fpc1020->input_dev->name = FPC1020_DEV_NAME;

		/* Set event bits according to what events we are generating */
		set_bit(EV_KEY, fpc1020->input_dev->evbit);
		set_bit(EV_REL, fpc1020->input_dev->evbit);
		input_set_capability(fpc1020->input_dev, EV_REL, REL_X);
		input_set_capability(fpc1020->input_dev, EV_REL, REL_Y);
		input_set_capability(fpc1020->input_dev, EV_KEY, BTN_MOUSE);
		input_set_capability(fpc1020->input_dev, EV_KEY, KEY_ENTER);

#ifdef VENDOR_EDIT
        //Lycan.Wang@Prd.BasicDrv, 2014-09-12 Add for report touch down or up
        set_bit(FPC1020_KEY_FINGER_TOUCH, fpc1020->input_dev->keybit);
        set_bit(FPC1020_KEY_FINGER_DTP, fpc1020->input_dev->keybit);
        set_bit(FPC1020_KEY_MOVE_FORWARD, fpc1020->input_dev->keybit);
        set_bit(FPC1020_KEY_MOVE_BACKWARD, fpc1020->input_dev->keybit);
        set_bit(FPC1020_KEY_ROTATE_FORWARD, fpc1020->input_dev->keybit);
        set_bit(FPC1020_KEY_ROTATE_BACKWARD, fpc1020->input_dev->keybit);


#ifdef AS_HOME_KEY
        set_bit(FPC1020_KEY_FINGER_PRESS, fpc1020->input_dev->keybit);
        //set_bit(FPC1020_KEY_FINGER_LONG_PRESS, fpc1020->input_dev->keybit);
        set_bit(FPC1020_KEY_FINGER_DOUBLE_TAB, fpc1020->input_dev->keybit);
        set_bit(KEY_POWER, fpc1020->input_dev->keybit);
        set_bit(KEY_F2, fpc1020->input_dev->keybit);
        fpc1020->to_power = false;
#endif

#endif /* VENDOR_EDIT */

		/* Register the input device */
		error = input_register_device(fpc1020->input_dev);


		if (error) {
			dev_err(&fpc1020->spi->dev, "Input_register_device failed.\n");
			input_free_device(fpc1020->input_dev);
			fpc1020->input_dev = NULL;
		}
	}

	fpc1020->touch_pad_dev = input_allocate_device();

	if (!fpc1020->touch_pad_dev) {
		dev_err(&fpc1020->spi->dev, "Input_allocate_device failed.\n");
		error  = -ENOMEM;
	}

	if (!error) {
		fpc1020->touch_pad_dev->name = FPC1020_TOUCH_PAD_DEV_NAME;

		/* Set event bits according to what events we are generating */
		set_bit(EV_KEY, fpc1020->touch_pad_dev->evbit);
		set_bit(EV_ABS, fpc1020->touch_pad_dev->evbit);
		set_bit(BTN_TOUCH, fpc1020->touch_pad_dev->keybit);
		set_bit(ABS_X, fpc1020->touch_pad_dev->absbit);
		set_bit(ABS_Y, fpc1020->touch_pad_dev->absbit);
		set_bit(ABS_Z, fpc1020->touch_pad_dev->absbit);
		input_set_abs_params(fpc1020->touch_pad_dev, ABS_X, 0, DEVICE_WIDTH, 0, 0);
		input_set_abs_params(fpc1020->touch_pad_dev, ABS_Y, 0, DEVICE_HEIGHT, 0, 0);

		/* Register the input device */
		error = input_register_device(fpc1020->touch_pad_dev);


		if (error) {
			dev_err(&fpc1020->spi->dev, "Input_register_device failed.\n");
			input_free_device(fpc1020->touch_pad_dev);
			fpc1020->touch_pad_dev = NULL;
		}
	}

	if (!error) {
		/* sub area setup */
		fpc1020->nav.image_nav_row_start = ((fpc1020->chip.pixel_rows - NAV_IMAGE_HEIGHT)/2);
		fpc1020->nav.image_nav_row_count = NAV_IMAGE_HEIGHT;
		fpc1020->nav.image_nav_col_start = ((fpc1020->chip.pixel_columns - NAV_IMAGE_WIDTH)/2) / fpc1020->chip.adc_group_size;
		fpc1020->nav.image_nav_col_groups = (NAV_IMAGE_WIDTH + fpc1020->chip.adc_group_size - 1) / fpc1020->chip.adc_group_size;

		//fpc1020->nav.input_mode = FPC1020_INPUTMODE_TRACKPAD;
#ifndef VENDOR_EDIT
		//Lycan.Wang@Prd.BasicDrv, 2014-09-29 Modify for navigation move event
		fpc1020->nav.input_mode = FPC1020_INPUTMODE_TOUCH;
#else /* VENDOR_EDIT */
		fpc1020->nav.input_mode = FPC1020_INPUTMODE_MOVE;
#ifndef AS_HOME_KEY
//		fpc1020->nav.enabled = false;
		init_enhanced_navi_setting(fpc1020);
#else
        fpc1020->nav.enabled = true;
#endif
#endif /* VENDOR_EDIT */
		
	}

	return error;
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
void /*__devexit*/ fpc1020_input_destroy(fpc1020_data_t *fpc1020)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->input_dev != NULL)
		input_free_device(fpc1020->input_dev);
	if (fpc1020->touch_pad_dev != NULL)
		input_free_device(fpc1020->touch_pad_dev);
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
int fpc1020_input_enable(fpc1020_data_t *fpc1020, bool enabled)
{
	//dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->nav.enabled = enabled;

	return 0;
}
#endif

#ifdef VENDOR_EDIT
//Lycan.Wang@Prd.BasicDrv, 2014-09-12 Add for report touch down or up
void fpc1020_report_finger_down(fpc1020_data_t *fpc1020)
{
    input_report_key(fpc1020->input_dev,
            FPC1020_KEY_FINGER_TOUCH, 1);

    input_sync(fpc1020->input_dev);
}

void fpc1020_report_finger_up(fpc1020_data_t *fpc1020)
{
    input_report_key(fpc1020->input_dev,
            FPC1020_KEY_FINGER_TOUCH, 0);

    input_sync(fpc1020->input_dev);
}


#endif /* VENDOR_EDIT */

#ifdef AS_HOME_KEY

/* -------------------------------------------------------------------- */
static int fpc1020_write_lpm_setup(fpc1020_data_t *fpc1020)
{
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	int error = 0;
	u16 temp_u16;
	fpc1020_reg_access_t reg;

	//dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	error = fpc1020_write_sensor_setup(fpc1020);
	if(error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


extern struct wake_lock fpc1020_wake_lock;
/* -------------------------------------------------------------------- */
static int fpc1020_wait_finger_present_lpm(fpc1020_data_t *fpc1020)
{
	const int lpm_poll_delay_ms = FPC1020_INPUT_POLL_TIME_MS;
#if 0
	const int zmask_5 = 1 << 5;
	const int zmask_6 = 1 << 6;
#endif
	const int zmask_ext = FPC1020_FINGER_DETECT_ZONE_MASK;

	int error = 0;
	int zone_raw = 0;

	bool wakeup_center = false;
	bool wakeup_ext    = false;
	bool wakeup        = false;

	int is_2050       = 0;
#if 0
    bool double_tap = false;
#endif
    bool finger_up = false;

	error = fpc1020_wake_up(fpc1020);

	if (!error)
		error = fpc1020_calc_finger_detect_threshold_min(fpc1020);

	if (error >= 0)
		error = fpc1020_set_finger_detect_threshold(fpc1020, error);

	if (error >= 0)
		error = fpc1020_write_lpm_setup(fpc1020);
//changhua remove for 12 zone to detect,will consume more 6ma,before is 250ua
    is_2050 = gpio_is_valid(fpc1020->fp2050_gpio) ? gpio_get_value(fpc1020->fp2050_gpio):0;
    if(is_2050)//with 2050,do sleep(0) with 5 6 zone detect home for antenna issue
    {

        if (!error) {
            error = fpc1020_sleep(fpc1020, false);

            if (error == -EAGAIN) {
                error = fpc1020_sleep(fpc1020, false);

                if (error == -EAGAIN)
                    error = 0;
            }
        }

    }

	while (!fpc1020->worker.stop_request && !error && !wakeup) {
		if (!error)
			error = fpc1020_wait_finger_present(fpc1020);

        /*dev_err(&fpc1020->spi->dev,
				"%s fpc1020_wait_finger_present return %d !\n", __func__,error);*/
		if (!error){
		    //if(fpc1020->to_power == true){
		    //    dev_err(&fpc1020->spi->dev,"%s Finger report Power KEY\n", __func__);
            //    input_report_key(fpc1020->input_dev,
			//			KEY_POWER, 1);
			//    input_sync(fpc1020->input_dev);
			//    input_report_key(fpc1020->input_dev,
			//			KEY_POWER, 0);
			//    input_sync(fpc1020->input_dev);
			//    error = 0;
			//}else{
                error = fpc1020_check_finger_present_raw(fpc1020);
                /*dev_err(&fpc1020->spi->dev,
				"%s fpc1020_check_finger_present_raw return %d !\n", __func__,error);*/
			//}
			
		}

		zone_raw = (error >= 0) ? error : 0;

#ifdef AS_HOME_KEY //add for fast tab by changhua
        /* //remove for single click become double click
        if (error == 0){
            dev_err(&fpc1020->spi->dev,"%s fast tab ---HOME DOWN !\n", __func__);
                input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 1);
			    input_sync(fpc1020->input_dev);
			finger_up = true;
            dev_err(&fpc1020->spi->dev,"%s fast tab --- HOME UP !\n", __func__);
            	input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 0);
			    input_sync(fpc1020->input_dev);
			wakeup = true;
        }*/
        if (error > 0) {
#else

		if (error >= 0) {
#endif
			error = 0;
#if 0
			wakeup_center = (zone_raw & zmask_5) ||
					(zone_raw & zmask_6);
#else
            wakeup_center = true;
#endif
			/* Todo: refined extended processing ? */
			wakeup_ext = ((zone_raw & zmask_ext) == zmask_ext);
			//changhua.li add for more condition
			if (zone_raw > fpc1020->setup.capture_finger_down_threshold)
			    wakeup_ext = true;

		} else {
			wakeup_center =
			wakeup_ext    = false;
		}

		if (wakeup_center && wakeup_ext) {
			dev_err(&fpc1020->spi->dev,
				"%s Wake up !\n", __func__);
#if 0
		//check for double tab,timeout in 100ms
		if(fpc1020_wait_finger_present_timeout(fpc1020) < 0/*==-ETIMEDOUT*/)
		{
                double_tap = false;
                dev_err(&fpc1020->spi->dev,"%s HOME DOWN !\n", __func__);
                input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 1);
			    input_sync(fpc1020->input_dev);

            	while (!finger_up && (error >= 0)) {

            		if (fpc1020->worker.stop_request)
            		{
            			error = -EINTR;
            			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 0);
			            input_sync(fpc1020->input_dev);
            		}
            		else
            			error = fpc1020_check_finger_present_sum(fpc1020);

            		if ((error >= 0) && (error < fpc1020->setup.capture_finger_up_threshold + 1))
            		{
            			finger_up = true;
            			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 0);
			            input_sync(fpc1020->input_dev);
            		}
            		else
            			msleep(FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS);
            	}
            } else
			{
			    double_tap = true;
			    dev_err(&fpc1020->spi->dev,"%s DOUBLE_TAB !\n", __func__);
			    input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_DOUBLE_TAB, 1);
			    input_sync(fpc1020->input_dev);
			    
			    input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_DOUBLE_TAB, 0);
			    input_sync(fpc1020->input_dev);
			}
#else
			dev_err(&fpc1020->spi->dev,"%s HOME DOWN !  fpc1020->to_power(%d)\n", __func__,fpc1020->to_power);
			if(fpc1020->to_power != true){
                input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 1);
			    input_sync(fpc1020->input_dev);
			}else if (enable_keys) {
			    wake_lock_timeout(&fpc1020_wake_lock,5*HZ);
			    input_report_key(fpc1020->input_dev,
						KEY_HOME, 1);
			    input_sync(fpc1020->input_dev);
			}
			while (!finger_up && (error >= 0)) {

            		if (fpc1020->worker.stop_request)
            		{
            			error = -EINTR;
                                /*
            			dev_err(&fpc1020->spi->dev,"%s HOME UP by stop requst !\n", __func__);
            			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 0);
			            input_sync(fpc1020->input_dev);
                               */
                        if(fpc1020->to_power == true){
                            dev_err(&fpc1020->spi->dev,"%s KEY UP by stop requst !\n", __func__);
                			input_report_key(fpc1020->input_dev,
    						KEY_HOME, 0);
    			            input_sync(fpc1020->input_dev);
    			            //wake_unlock(&fpc1020_wake_lock);
                        }
            		}
            		else{
            		    //fpc1020_reset(fpc1020);//changhua add for can not detect finger leave in some IC with dead pixel
                        //fpc1020_write_capture_setup(fpc1020);//changhua add for can not detect finger leave in some IC with dead pixel

            			error = fpc1020_check_finger_present_sum(fpc1020);
            			}

            		if ((error >= 0) && (error < fpc1020->setup.capture_finger_up_threshold + 1))//changhua modify from 1-->2 for iron ball&fall down test damage IC(dead pixel [18,33])
            		{
            			finger_up = true;
            			dev_err(&fpc1020->spi->dev,"%s HOME UP !  fpc1020->to_power(%d)\n", __func__,fpc1020->to_power);
                        if(fpc1020->to_power != true){
                            input_report_key(fpc1020->input_dev,
                                FPC1020_KEY_FINGER_PRESS, 0);
                            input_sync(fpc1020->input_dev);
                        }else if (enable_keys) {
                            input_report_key(fpc1020->input_dev,
                                KEY_HOME, 0);
                            input_sync(fpc1020->input_dev);
                            //wake_unlock(&fpc1020_wake_lock);
                        }
            		}
            		else
            			msleep(FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS);
            	}
#endif

			wakeup = true;
		}
		if (!wakeup && !error) {
			error = fpc1020_sleep(fpc1020, false);

			if (error == -EAGAIN)
				error = 0;

			if (!error)
				msleep(lpm_poll_delay_ms);
		}
	}

	/*if (error < 0)
		dev_err(&fpc1020->spi->dev,
			"%s %s %d!\n", __func__,
			(error == -EINTR) ? "TERMINATED" : "FAILED", error);*/

	return error;
}
#endif/*AS_HOME_KEY*/
/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
int fpc1020_input_task(fpc1020_data_t *fpc1020)
{
#ifndef AS_HOME_KEY
	bool isReverse = false;
	int dx = 0;
	int dy = 0;
	int sumX = 0;
	int sumY = 0;
	int error = 0;
	unsigned char* prevBuffer = NULL;
	unsigned char* curBuffer = NULL;
	unsigned long diffTime = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_write_nav_setup(fpc1020);

	while (!fpc1020->worker.stop_request &&
		fpc1020->nav.enabled && (error >= 0)) {

		error = fpc1020_capture_wait_finger_down(fpc1020);
		if (error < 0)
			break;

        error = fpc1020_write_nav_setup(fpc1020);
        if (error < 0)
        { break; }
        error = fpc1020_check_finger_present_raw(fpc1020);
//
		process_navi_event(fpc1020, 0, 0, FNGR_ST_DETECTED);
//
                fpc1020->nav.detect_zones = error & 0xFFF;
		error = capture_nav_image(fpc1020);
		if(error < 0)
			break;

		memcpy(fpc1020->prev_img_buf, fpc1020->huge_buffer, NAV_IMAGE_WIDTH * NAV_IMAGE_HEIGHT);

		while (!fpc1020->worker.stop_request && (error >= 0)) {

			if(isReverse) {
				prevBuffer = fpc1020->cur_img_buf;
				curBuffer = fpc1020->prev_img_buf;
			} else {
				prevBuffer = fpc1020->prev_img_buf;
				curBuffer = fpc1020->cur_img_buf;
			}
			error = capture_nav_image(fpc1020);
			if(error < 0)
				break;

			memcpy(curBuffer, fpc1020->huge_buffer, NAV_IMAGE_WIDTH * NAV_IMAGE_HEIGHT);
			error = fpc1020_check_finger_present_sum(fpc1020);
			if (error < fpc1020->setup.capture_finger_up_threshold + 1) {
				process_navi_event(fpc1020, 0, 0, FNGR_ST_LOST);
				sumX = 0;
				sumY = 0;
				fpc1020->nav.time = 0;
				isReverse = false;
				break;
			}

			isReverse = !isReverse;
			get_movement(prevBuffer, curBuffer, &dx, &dy);

			sumX += dx;
			sumY += dy;

            pr_info("[FPC] get_movement dx=%d, dy=%d, sumx=%d, sumy=%d\n", dx, dy, sumX, sumY);

			diffTime = abs(jiffies - fpc1020->nav.time);
			if(diffTime > 0) {
				diffTime = diffTime * 1000000 / HZ;

				if (diffTime >= FPC1020_INPUT_POLL_INTERVAL) {
					process_navi_event(fpc1020, sumX, sumY, FNGR_ST_MOVING);
					dev_info(&fpc1020->spi->dev, "[INFO] nav finger moving. sumX = %d, sumY = %d\n", sumX, sumY);
					sumX = 0;
					sumY = 0;
					fpc1020->nav.time = jiffies;
				}
			}

		}
	}

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s %s (%d)\n",
			__func__,
			(error == -EINTR) ? "TERMINATED" : "FAILED", error);
	}

#ifndef VENDOR_EDIT
	//Lycan.Wang@Prd.BasicDrv, 2014-09-28 Remove for nav disabled when out of suspend
        atomic_set(&fpc1020->taskstate, fp_UNINIT);

	fpc1020->nav.enabled = false;
#endif /* VENDOR_EDIT */


	return error;


#else// AS_HOME_KEY
int error = 0;

	//dev_err(&fpc1020->spi->dev, "%s\n", __func__);

	while (!fpc1020->worker.stop_request &&
		fpc1020->nav.enabled /*&& !error*/) {

		error = fpc1020_wait_finger_present_lpm(fpc1020);
        /*
		//add_timer(&s_timer);
		if (error == 0) {		    
			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 1);
			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESS, 0);

			input_sync(fpc1020->input_dev);
		}*/
		if (error < 0 && error != -EINTR){
		    msleep(10);
		}
	}
	/*if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s %s (%d)\n",
			__func__,
			(error == -EINTR) ? "TERMINATED" : "FAILED", error);
	}*/
	return error;

#endif// AS_HOME_KEY
}
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#ifndef AS_HOME_KEY
static int fpc1020_write_nav_setup(fpc1020_data_t *fpc1020)
{
	const int mux = 2;
	int error = 0;
	u16 temp_u16;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	error = fpc1020_wake_up(fpc1020);
	if (error)
		goto out;

	error = fpc1020_write_sensor_setup(fpc1020);
	if(error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	error = fpc1020_capture_set_crop(fpc1020,
				fpc1020->nav.image_nav_col_start, fpc1020->nav.image_nav_col_groups,
				fpc1020->nav.image_nav_row_start, fpc1020->nav.image_nav_row_count);
	if (error)
		goto out;

	dev_dbg(&fpc1020->spi->dev, "%s, (%d, %d, %d, %d)\n", __func__,
				fpc1020->nav.image_nav_col_start, fpc1020->nav.image_nav_col_groups,
				fpc1020->nav.image_nav_row_start, fpc1020->nav.image_nav_row_count);

out:
	return error;
}
#endif
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#if 0
static int fpc1020_write_lpm_setup(fpc1020_data_t *fpc1020)
{
	return fpc1020_write_sensor_setup(fpc1020);
}
#endif
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#if 0
static int fpc1020_wait_finger_present_lpm(fpc1020_data_t *fpc1020)
{
	const int lpm_poll_delay_ms = FPC1020_INPUT_POLL_TIME_MS;
	const int zmask_5 = 1 << 5;
	const int zmask_6 = 1 << 6;
	const int zmask_ext = FPC1020_FINGER_DETECT_ZONE_MASK &
				FPC1020_HW_DETECT_MASK;

	int error = 0;
	int zone_raw = 0;

	bool wakeup_center = false;
	bool wakeup_ext    = false;
	bool wakeup        = false;

	error = fpc1020_write_lpm_setup(fpc1020);

	if (!error) {
		error = fpc1020_sleep(fpc1020, false);

		if (error == -EAGAIN) {
			error = fpc1020_sleep(fpc1020, false);

			if (error == -EAGAIN)
				error = 0;
		}
	}

	while (!fpc1020->worker.stop_request && !error && !wakeup) {
		if (!error)
			error = fpc1020_wait_finger_present(fpc1020);

		if (!error)
			error = fpc1020_check_finger_present_raw(fpc1020);

		zone_raw = (error >= 0) ? error : 0;

		if (error >= 0) {
			error = 0;

			wakeup_center = (zone_raw & zmask_5) ||
					(zone_raw & zmask_6);

 			/* Todo: refined extended processing ? */
			wakeup_ext = ((zone_raw & zmask_ext) == zmask_ext);

		} else {
			wakeup_center =
			wakeup_ext    = false;
		}

		if (wakeup_center && wakeup_ext) {
			dev_dbg(&fpc1020->spi->dev,
				"%s Wake up !\n", __func__);
			wakeup = true;
		}
		if (!wakeup && !error) {
			error = fpc1020_sleep(fpc1020, false);

			if (error == -EAGAIN)
				error = 0;

			if (!error)
				msleep(lpm_poll_delay_ms);
		}
	}

	if (!error)
		error = fpc1020_write_nav_setup(fpc1020);

	if (error < 0)
		dev_dbg(&fpc1020->spi->dev,
			"%s FAILED %d!\n", __func__,
			error);

	return error;
}
#endif
#endif


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
#ifndef AS_HOME_KEY
static int capture_nav_image(fpc1020_data_t *fpc1020)
{
	int error = 0;
	size_t image_size = 0;

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0) {
		dev_dbg(&fpc1020->spi->dev, "%s, fpc1020_read_irq error\n", __func__);
	}
	
	//dev_dbg(&fpc1020->spi->dev, "%s, !!!!!(%d, %d, %d, %d)!!!!!\n", __func__,
	//			fpc1020->nav.image_nav_col_start, fpc1020->nav.image_nav_col_groups,
	//			fpc1020->nav.image_nav_row_start, fpc1020->nav.image_nav_row_count);
	
	image_size = fpc1020->nav.image_nav_row_count *
                     fpc1020->nav.image_nav_col_groups * fpc1020->chip.adc_group_size;

	error = fpc1020_capture_buffer(fpc1020,
					fpc1020->huge_buffer,
					0,
					image_size);

	return error;
}
#endif
#endif


/* -------------------------------------------------------------------- */

