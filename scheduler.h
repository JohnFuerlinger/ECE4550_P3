#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <projdefs.h>
#include <timers.h>
#include <list.h>
#include <croutine.h>
#include <portable.h>
#include <stack_macros.h>
#include <mpu_wrappers.h>
#include <FreeRTOSVariant.h>
#include <message_buffer.h>
#include <semphr.h>
#include <FreeRTOSConfig.h>
#include <stream_buffer.h>
#include <portmacro.h>
#include <event_groups.h>
#include <queue.h>
#include <Arduino.h>



#ifdef __cplusplus
extern "C" {
#endif

#define PRINT_HEADER					1 /** Basic debug info that gets printed once when everthing gets created */
#define PRINT_SCHEDULER_DEBUG			1 /** Debugging info related to what the scheduler is doing. **/
#define PRINT_TASK_DEBUG				1 /** Debugging info in the actual tasks **/

/** These are the supported scheduling algorithms **/
#define schedSCHEDULING_POLICY_DMS		0
#define schedSCHEDULING_POLICY_RMS		1
#define schedSCHEDULING_POLICY_EDF		2
#define schedSCHEDULING_POLICY_HVDF		3

/** NOTE: UPDATE THE BELOW TO SELECT SCHEDULING POLICY **/
#define schedSCHEDULING_POLICY schedSCHEDULING_POLICY_RMS

/* Maximum number of periodic tasks that can be created. (Scheduler task is
 * not included) */
#define schedMAX_NUMBER_OF_PERIODIC_TASKS 5

/* Set this define to 1 to enable Timing-Error-Detection for detecting tasks
 * that have missed their deadlines. Tasks that have missed their deadlines
 * will be deleted, recreated and restarted during next period. */
#define schedUSE_TIMING_ERROR_DETECTION_DEADLINE 1

/* Set this define to 1 to enable Timing-Error-Detection for detecting tasks
 * that have exceeded their worst-case execution time. Tasks that have exceeded
 * their worst-case execution time will be preempted until next period. */
#define schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME 1

/* Set this define to 1 to enable the scheduler task. This define must be set to 1
* when using following features:
* EDF scheduling policy, Timing-Error-Detection of execution time,
* Timing-Error-Detection of deadline, Polling Server. */
#define schedUSE_SCHEDULER_TASK 1


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Priority of the scheduler task. */
	#define schedSCHEDULER_PRIORITY ( configMAX_PRIORITIES - 1 )
	/* Stack size of the scheduler task. */
	#define schedSCHEDULER_TASK_STACK_SIZE 200 
	/* The period of the scheduler task in software ticks. */
	#define schedSCHEDULER_TASK_PERIOD pdMS_TO_TICKS( 100 )			/** So.. The scheduler task is running with T = 0.1 seconds..?? Seems pretty slow **/
#endif /* schedUSE_SCHEDULER_TASK */

/* This function must be called before any other function call from scheduler.h. */
void vSchedulerInit( void );

/* Creates a periodic task.
 *
 * pvTaskCode: The task function.
 * pcName: Name of the task.
 * usStackDepth: Stack size of the task in words, not bytes.
 * pvParameters: Parameters to the task function.
 * uxPriority: Priority of the task. (Only used when scheduling policy is set to manual)
 * pxCreatedTask: Pointer to the task handle.
 * xPhaseTick: Phase given in software ticks. Counted from when vSchedulerStart is called.
 * xPeriodTick: Period given in software ticks.
 * xMaxExecTimeTick: Worst-case execution time given in software ticks.
 * xDeadlineTick: Relative deadline given in software ticks.
 *
 * NEW: UBaseType_t xValue
 * */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick, UBaseType_t xValue );

/* Deletes a periodic task associated with the given task handle. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle );

/* Starts scheduling tasks. */
void vSchedulerStart( void );

#ifdef __cplusplus
}
#endif


#endif /* SCHEDULER_H_ */
