#include "scheduler.h"

#include "task.h"

#define schedUSE_TCB_ARRAY 1

/** TODO: Add/subtract from this as needed. It needs to be cleaned up **/

/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
	TaskFunction_t pvTaskCode; 		/* Function pointer to the code that will be run periodically. */
	const char *pcName; 			/* Name of the task. */
	UBaseType_t uxStackDepth;       /* Stack size of the task. */
	void *pvParameters; 			/* Parameters to the task function. */
	UBaseType_t uxPriority; 		/* Priority of the task. */
	TaskHandle_t *pxTaskHandle;		/* Task handle for the task. */
	TickType_t xReleaseTime;		/* Release time of the task. */
	TickType_t xRelativeDeadline;	/* Relative deadline of the task. */
	TickType_t xAbsoluteDeadline;	/* Absolute deadline of the task. */
	TickType_t xPeriod;				/* Task period. */
	TickType_t xLastWakeTime; 		/* Last time stamp when the task was running. */
	TickType_t xMaxExecTime;		/* Worst-case execution time of the task. */
	TickType_t xExecTime;			/* Current execution time of the task. */

	BaseType_t xWorkIsDone; 		/* pdFALSE if the job is not finished, pdTRUE if the job is finished. */
	
	UBaseType_t xValue;				/** Value of the task 'V' **/

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xPriorityIsSet; 	/* pdTRUE if the priority is assigned. */
		BaseType_t xInUse; 			/* pdFALSE if this extended TCB is empty. */
	#endif

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		BaseType_t xExecutedOnce;	/* pdTRUE if the task has executed once. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		BaseType_t xSuspended; 		/* pdTRUE if the task is suspended. */
		BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
	/* add if you need anything else */	
	
} SchedTCB_t;



#if( schedUSE_TCB_ARRAY == 1 )
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle );
	static void prvInitTCBArray( void );
	/* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void );
	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex );
#endif /* schedUSE_TCB_ARRAY */

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode( void *pvParameters );
static void prvCreateAllTasks( void );


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	static void prvSetFixedPriorities( void );	
#endif /* schedSCHEDULING_POLICY_RMS */

static void prvSetInitialDeadlines( void );

#if( schedUSE_SCHEDULER_TASK == 1 )
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB );
	static void prvSchedulerFunction( void *pvParameters);
	static void prvCreateSchedulerTask( void );
	static void prvWakeScheduler( void );

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB );
		static void prvDeadlineMissedHook( SchedTCB_t *pxTCB);
		static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount );				
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		static void prvExecTimeExceedHook( SchedTCB_t *pxCurrentTask );
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
#endif /* schedUSE_SCHEDULER_TASK */



#if( schedUSE_TCB_ARRAY == 1 )
	/* Array for extended TCBs. */
	static SchedTCB_t xTCBArray[ schedMAX_NUMBER_OF_PERIODIC_TASKS ];
	/* Counter for number of periodic tasks. */
	static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

/** TODO: This could be used to track overhead **/
#if( schedUSE_SCHEDULER_TASK )
	static TickType_t xSchedulerWakeCounter = 0; /* useful. why? */
	static TaskHandle_t xSchedulerHandle = NULL; /* useful. why? */
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_TCB_ARRAY == 1 )
	/* Returns index position in xTCBArray of TCB with same task handle as parameter. */
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle )
	{
		static BaseType_t xIndex = 0;
		BaseType_t xIterator;

		for( xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
		{
		
			if( pdTRUE == xTCBArray[ xIndex ].xInUse && *xTCBArray[ xIndex ].pxTaskHandle == xTaskHandle )
			{
				return xIndex;
			}
		
			xIndex++;
			if( schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex )
			{
				xIndex = 0;
			}
		}
		return -1;
	}

	/* Initializes xTCBArray. */
	static void prvInitTCBArray( void )
	{
	UBaseType_t uxIndex;
		for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
		{
			xTCBArray[ uxIndex ].xInUse = pdFALSE;
		}
	}

	/* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void )
	{
		/* your implementation goes here */
		UBaseType_t uxIndex;
		for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
		{
			//check for an element that is not in use, return it if found
			if(xTCBArray[ uxIndex ].xInUse == pdFALSE)
				return uxIndex;
		}
		return -1;
	}

	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex )
	{
		/* your implementation goes here */
		//Make sure the index is valid before checking that the element is in use so we dont crash
		configASSERT( xIndex >= 0 && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS);
		//Make sure the element is in use before changing it for no reason
		configASSERT(xTCBArray[ xIndex ].xInUse == pdTRUE);

		//Set the element as not in use and decrement the number of tasks
		xTCBArray[ xIndex ].xInUse = pdFALSE;
		xTaskCounter--;
	}
	
#endif /* schedUSE_TCB_ARRAY */

/**  TODO: This should be cleaned up. I want to get tasks actually running at or 
 **  close to their WCET's
 **/

/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode( void *pvParameters )
{
	SchedTCB_t *pxThisTask;
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();
	    
    /* Check the handle is not NULL. */
	configASSERT(xCurrentTaskHandle != NULL);
	
	/** Much easier than iterating over the xTCBArray **/
	BaseType_t xIndex;
	xIndex = prvGetTCBIndexFromHandle(xCurrentTaskHandle);
	pxThisTask = &xTCBArray[ xIndex ];
	
	if (pxThisTask->xReleaseTime) {
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);
	}
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
        /* your implementation goes here */
			//Set this task as ExecutedOnce for checking whether it has been executed but not finished later
			pxThisTask->xExecutedOnce = pdTRUE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
    
	if( 0 == pxThisTask->xReleaseTime )
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}

	for( ; ; )
	{	
		pxThisTask->pvTaskCode( pvParameters );
		
		/** We want to update the next xAbsoluteDeadline as soon as the task is finished
		 ** so that if it's a close call to missing it's deadline, we won't get a false
		 ** positive on a missed deadline detection.
		 **
		 ** This shouldn't be required but adds another layer of projection to ensure that
		 ** we're checking the correct deadlines for each task.
		 **/
		pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xRelativeDeadline;
		
		pxThisTask->xWorkIsDone = pdTRUE;
		
		
				
		pxThisTask->xExecTime = 0;
        
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod);
	}
}

/** TODO: This will require modification as new vars are added to the TCB struct **/

/** NOTE: Also should clean it up **/

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick, UBaseType_t xValue )
{
	taskENTER_CRITICAL();
	SchedTCB_t *pxNewTCB;
	
	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex = prvFindEmptyElementIndexTCB();
		configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
		configASSERT( xIndex != -1 );
		pxNewTCB = &xTCBArray[ xIndex ];	
	#endif /* schedUSE_TCB_ARRAY */

	/* Intialize item. */
		
	pxNewTCB->pvTaskCode = pvTaskCode;
	pxNewTCB->pcName = pcName;
	pxNewTCB->uxStackDepth = uxStackDepth;
	pxNewTCB->pvParameters = pvParameters;
	pxNewTCB->uxPriority = uxPriority;
	pxNewTCB->pxTaskHandle = pxCreatedTask;
	pxNewTCB->xReleaseTime = xPhaseTick;
	pxNewTCB->xPeriod = xPeriodTick;
	
	pxNewTCB->xValue = xValue;
	
    /* Populate the rest */
    /* your implementation goes here */
	
	pxNewTCB->xRelativeDeadline = xDeadlineTick;
	pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
	pxNewTCB->xWorkIsDone = pdFALSE;
    
	#if( schedUSE_TCB_ARRAY == 1 )
		pxNewTCB->xInUse = pdTRUE;
	#endif /* schedUSE_TCB_ARRAY */
	
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
		/* member initialization */
        /* your implementation goes here */
		pxNewTCB->xPriorityIsSet = pdFALSE;
	#endif /* schedSCHEDULING_POLICY */	
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		/* member initialization */
        /* your implementation goes here */
		pxNewTCB->xExecutedOnce = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        pxNewTCB->xSuspended = pdFALSE;
        pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */	

	#if( schedUSE_TCB_ARRAY == 1 )
		xTaskCounter++;	
	#endif /* schedUSE_TCB_SORTED_LIST */
	taskEXIT_CRITICAL();
	
	#if PRINT_HEADER
		/** Tell user various info when tasks are created **/	
		char strBuf[100];
		sprintf(strBuf, "Creating task: %s with C = %d, T = %d, D = %d   (all units in tics)", pxNewTCB->pcName, pxNewTCB->xMaxExecTime, pxNewTCB->xPeriod, pxNewTCB->xRelativeDeadline);
		Serial.println(strBuf);	
	#endif

}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle )
{
	/* your implementation goes here */
	#if( schedUSE_TCB_ARRAY == 1 )
	//Get the element to remove
	BaseType_t indexToRemove = prvGetTCBIndexFromHandle( xTaskHandle );
	//Take the TCB out of the array
	prvDeleteTCBFromArray( indexToRemove);
	#endif
	vTaskDelete( xTaskHandle );
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks( void )
{
	SchedTCB_t *pxTCB;

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex;
		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );
			pxTCB = &xTCBArray[ xIndex ];

			xTaskCreate((TaskFunction_t)prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle);
					
		}	
	#endif /* schedUSE_TCB_ARRAY */
}

/** This func is only called once (I think..?). It was previously used for setting fixed
 ** priorities according to RMS/DMS.It can probably be reworked to assign initial priorities
 ** for other scheduling algorithms
 **/

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	/* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
static void prvSetFixedPriorities( void )
{
	#if PRINT_HEADER
		#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS)
			Serial.println("\nSetting fixed priorities (RMS)...");
		#elif (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
			Serial.println("\nSetting fixed priorities (DMS)...");
		#endif	
	#endif /* PRINT_HEADER */
	
	
	BaseType_t xIter, xIndex;
	BaseType_t xTrackIndex;
	TickType_t xHighest;
	SchedTCB_t *pxTCB;
	
	/** We always want the highest task priority to be 1 less than whatever
	 ** the scheduler priority is set to.
	 **/
	UBaseType_t lowest_prio = 1;

	for( xIter = 0; xIter < xTaskCounter; xIter++ )
	{
		/** In this loop, we're finding a highest value **/
		
		xHighest = 0;
		xTrackIndex = 0;

		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			/** In this loop, we track the index of the highest we found **/
			
			pxTCB = &xTCBArray[xIndex];
			
			if (pxTCB->xPriorityIsSet == pdFALSE) { /** Skip over tasks that we've already checked **/
				#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS)
					if (pxTCB->xPeriod > xHighest) {
						xTrackIndex = xIndex;
						xHighest = pxTCB->xPeriod;
					}
				#elif (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
					if (pxTCB->xRelativeDeadline > xHighest) {
						xTrackIndex = xIndex;
						xHighest = pxTCB->xRelativeDeadline;
					}
				#endif				
			}
		}
		xTCBArray[xTrackIndex].uxPriority = lowest_prio;
		xTCBArray[xTrackIndex].xPriorityIsSet = pdTRUE;
		lowest_prio++;
	}
	
	/** Printing out each tasks priority after this initial set... **/
	#if (PRINT_HEADER)
		char strBuf[50];
		for (xIter = 0; xIter < xTaskCounter; xIter++) {
			sprintf(strBuf, "Setting: %s priority: %d", xTCBArray[xIter].pcName, xTCBArray[xIter].uxPriority);
			Serial.println(strBuf);
		}
		Serial.println("---------------END OF HEADER-----------------");
	#endif
}

#endif /* schedSCHEDULING_POLICY */

/** I don't think xAbsoluteDeadline is being initialized anywhere. This should 
 ** be called when vSchedulerStart is called, not when the tasks are created.
 **/

static void prvSetInitialDeadlines( void ) {
	
	BaseType_t xIter;
	TickType_t curTime = xTaskGetTickCount();
	
	for( xIter = 0; xIter < xTaskCounter; xIter++ )	{
		xTCBArray[xIter].xAbsoluteDeadline = curTime + xTCBArray[xIter].xRelativeDeadline;
	}
}

/** NOTE: This was one of the most troubling functions in P2. This function should get a
 ** little extra attention this time around to find any potentially lingering bugs, 
 ** and also thoroughly cleaned up.
 **/

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
				

		
		BaseType_t xReturnValue = xTaskCreate((TaskFunction_t)prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle);
				                      		
		if( pdPASS == xReturnValue )
		{
			/* your implementation goes here */
			
			// deadline = releasetime + period
			
			//TickType_t curTick = xTaskGetTickCount(); curtick unused
			
			
			pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
			
			
			pxTCB->xExecutedOnce = pdFALSE;
			pxTCB->xMaxExecTimeExceeded = pdFALSE;		
			pxTCB->xSuspended = pdFALSE;			
			
			#if PRINT_SCHEDULER_DEBUG
				Serial.println("\t(Sch) Recreation was successful.");
			#endif
		}
		else
		{
			/* if task creation failed */
			#if PRINT_SCHEDULER_DEBUG
				Serial.println("\t(Sch) Recreate task failed.");
			#endif
		}
	}
	
	/** NOTE: So long as this was working properly before, this shouldn't be any
	 ** different from RMS/DMS in contrast to the new schedulers
	 **/

	/* Called when a deadline of a periodic task is missed.
	 * Deletes the periodic task that has missed it's deadline and recreate it.
	 * The periodic task is released during next period. */
	static void prvDeadlineMissedHook( SchedTCB_t *pxTCB)
	{
		pxTCB->xSuspended = pdTRUE;
		
		#if PRINT_SCHEDULER_DEBUG
			char strBuf[50];
			sprintf(strBuf, "\n\t(Sch) MISSED DL: %s\n", pxTCB->pcName);
			Serial.println(strBuf);
		#endif

		vTaskDelete( *pxTCB->pxTaskHandle );
		pxTCB->xExecTime = 0;
		prvPeriodicTaskRecreate( pxTCB );	
		
		/* Need to reset next WakeTime for correct release. */
		/* your implementation goes here */
		
		//Reset the wake time and absolute deadline again
		pxTCB->xLastWakeTime = 0;
		//Now that the last wake time has been reset, use the release time instead of wake time 
		//since release time is now time to release instead of wake time
		pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{ 
		/* check whether deadline is missed. */     		
		/* your implementation goes here */
		
		/** let's assume that before this point, the absolute deadline
		 ** has already been calculated for pxTCB. I believe... the absolute
		 ** deadline can be calculated by:
		 ** xAbsoluteDeadline = xRelativeDeadline + ... actually not sure about this yet
		 **
		 ** Note: It looks like xAbsoluteDeadline currently isn't touched anywhere right now
		 **/
		#if PRINT_SCHEDULER_DEBUG
			char msgBuf[50];
			sprintf(msgBuf, "\t(Sch) Chk DL: %s, DL: %d, Cur Time: %d", pxTCB->pcName, pxTCB->xAbsoluteDeadline, xTickCount);
			Serial.println(msgBuf);
		#endif
		 
		/** Update the xAbsoluteDeadline **/
		pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;
		
		#if PRINT_SCHEDULER_DEBUG
			sprintf(msgBuf, "\t(Sch) Update DL: %d", pxTCB->xAbsoluteDeadline);
			Serial.println(msgBuf);
		#endif
		 
		/** we only want to call the below hook when we actually miss a deadline **/
		if (xTickCount > pxTCB->xAbsoluteDeadline && pxTCB->xWorkIsDone == pdFALSE && pxTCB->xExecutedOnce == pdTRUE) {
			prvDeadlineMissedHook( pxTCB );
		}
	}	
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

	/* Called if a periodic task has exceeded its worst-case execution time.
	 * The periodic task is blocked until next period. A context switch to
	 * the scheduler task occur to block the periodic task. */
	static void prvExecTimeExceedHook( SchedTCB_t *pxCurrentTask )
	{
		Serial.println("Time exceeded hook.");
        pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;
        /* Is not suspended yet, but will be suspended by the scheduler later. */
        pxCurrentTask->xSuspended = pdTRUE;
        pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
        pxCurrentTask->xExecTime = 0;
        
        BaseType_t xHigherPriorityTaskWoken;
        vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
        xTaskResumeFromISR(xSchedulerHandle);
	}
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Called by the scheduler task. Checks all tasks for any enabled
	 * Timing Error Detection feature. */
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB )
	{
		/* your implementation goes here */
		
		//No need to check if the task is not in use (i.e. it has been "deleted" from the array)
		if(pxTCB->xInUse == pdTRUE)
		{
			#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )						
				/* check if task missed deadline */
				/* your implementation goes here */
				//need to set workDone as false if the deadline is wrong
				//We know that since this task is in use, and the current tick is past the last wake time,
				//the deadline has been missed
				if(xTickCount > pxTCB->xLastWakeTime)
					pxTCB->xWorkIsDone = pdFALSE;
				prvCheckDeadline( pxTCB, xTickCount );						
			#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
			

			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				#if PRINT_SCHEDULER_DEBUG
					char msgBuf[50];
					sprintf(msgBuf, "\t(Sch) Chk WCET: %s", pxTCB->pcName);
					Serial.println(msgBuf);
				#endif
			if( pdTRUE == pxTCB->xMaxExecTimeExceeded )
			{
				pxTCB->xMaxExecTimeExceeded = pdFALSE;
				vTaskSuspend( *pxTCB->pxTaskHandle );
			}
			if( pdTRUE == pxTCB->xSuspended )
			{
				if( ( signed ) ( pxTCB->xAbsoluteUnblockTime - xTickCount ) <= 0 )
				{
					pxTCB->xSuspended = pdFALSE;
					pxTCB->xLastWakeTime = xTickCount;
					vTaskResume( *pxTCB->pxTaskHandle );
				}
			}
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
		}
		return;
	}

	/* Function code for the scheduler task. */
	static void prvSchedulerFunction( void *pvParameters )
	{
		(void)pvParameters; /** Get rid of compiler warning **/
		
		#if PRINT_SCHEDULER_DEBUG
			Serial.println("\n\t(Sch) Starting the Scheduler task");
		#endif
		for( ; ; )
		{ 
     		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				TickType_t xTickCount = xTaskGetTickCount();
        		SchedTCB_t *pxTCB;							
				
				//Need to check the set of tasks for timing
				#if( schedUSE_TCB_ARRAY == 1 )
					UBaseType_t xIndex;
					
					#if PRINT_SCHEDULER_DEBUG
						Serial.println("\t(Sch) timing checks...");
					#endif
					
					for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
					{
						pxTCB = &xTCBArray[ xIndex ];
						
						//Check task for timing errors
						prvSchedulerCheckTimingError( xTickCount, pxTCB );
					}
				#endif /* schedUSE_TCB_ARRAY */
			
			#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
			
			ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		}
	}

	/* Creates the scheduler task. */
	static void prvCreateSchedulerTask( void )
	{
		xTaskCreate( (TaskFunction_t) prvSchedulerFunction, "Scheduler", schedSCHEDULER_TASK_STACK_SIZE, NULL, schedSCHEDULER_PRIORITY, &xSchedulerHandle );                             
	}
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Wakes up (context switches to) the scheduler task. */
	static void prvWakeScheduler( void )
	{
		#if PRINT_SCHEDULER_DEBUG
			Serial.println("\n\twaking scheduler...");
		#endif
		BaseType_t xHigherPriorityTaskWoken;
		vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
		xTaskResumeFromISR(xSchedulerHandle);
	}
	
	/** TODO: Shouldn't we only be relying on the scheduler task to be doing these checks for us..?
	 ** This feels like an intrusion of user-space. If anything, we should increase the frequency
	 ** of the scheduler task to every tick if we need to check that often. Even better, I should
	 ** determing the critical times to check these errors and then only wake at those times.
	 **/

	/* Called every software tick. */
	void vApplicationTickHook( void )
	{
		TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();
		BaseType_t taskindex = prvGetTCBIndexFromHandle( xCurrentTaskHandle );
		SchedTCB_t *pxCurrentTask = &xTCBArray[ taskindex ];
		UBaseType_t prioCurrentTask = pxCurrentTask->uxPriority;
        UBaseType_t flag = 0;
        BaseType_t xIndex;
    
        for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
        {
            pxCurrentTask = &xTCBArray[ xIndex ];
            if( pxCurrentTask->uxPriority == prioCurrentTask ){
                flag = 1;
                break;
            }
        }
		if( xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle() && flag == 1)
		{
			pxCurrentTask->xExecTime++;     
		
			/** NOTE: changed operator to be > instead of >= to achieve correct behavior. 
			 ** This allows each task to execute at exactly its WCET and not fail these timing
			 ** checks and be suspended. We only want to suspend tasks if they EXCEED their 
			 ** WCET, not if they are currently at the WCET.
			 **/
			 
			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
            if( pxCurrentTask->xExecTime > pxCurrentTask->xMaxExecTime )
            {
                if( pdFALSE == pxCurrentTask->xMaxExecTimeExceeded )
                {
                    if( pdFALSE == pxCurrentTask->xSuspended )
                    {
                        prvExecTimeExceedHook( pxCurrentTask );
                    }
                }
            }
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
		}

		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )    
			xSchedulerWakeCounter++;      
			if( xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD )
			{
				xSchedulerWakeCounter = 0;        
				prvWakeScheduler();
			}
		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	}
#endif /* schedUSE_SCHEDULER_TASK */

/* This function must be called before any other function call from this module. */
void vSchedulerInit( void )
{
	#if PRINT_HEADER
		#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS)
			Serial.println("Init scheduler (RMS)\n");
		#elif (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
			Serial.println("Init scheduler (DMS)\n");
		#endif
		
		/** Note: When I tested this, I found that tics occur at a frequency of 62 hz **/
		
		/** TODO: 62 Hz is absurd. This should be much faster I think. I think the best thing
		 ** would be to figure out the system clock rate and then determine the proper tick rate
		 ** as well as scheduler wake frequency. Note that the Mega has a 16Mhz XTAL on it, so 
		 ** the system clock speed is likely some prescalar of that.
		 **/
		char strBuf[50];
		sprintf(strBuf, "FreeRTOS TickRate period == 1/%d seconds", configTICK_RATE_HZ);
		Serial.println(strBuf);
		Serial.println();
	#endif /* PRINT_HEADER */
	
		
	#if( schedUSE_TCB_ARRAY == 1 )
		prvInitTCBArray();
	#endif /* schedUSE_TCB_ARRAY */
}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart( void )
{
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
		prvSetFixedPriorities();	
	#endif /* schedSCHEDULING_POLICY */
	
	/** Set initial deadlines */
	prvSetInitialDeadlines();

	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();
	  
	xSystemStartTime = xTaskGetTickCount();
	
	vTaskStartScheduler();
}
