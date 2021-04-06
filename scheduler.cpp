#include "scheduler.h"

#include "task.h"

#define schedUSE_TCB_ARRAY 1

extern TaskHandle_t xHandle1;
extern TaskHandle_t xHandle2;
extern TaskHandle_t xHandle3;
extern TaskHandle_t xHandle4;

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
	float xValueDensity;

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

void prvSetFixedPriorities( void );

void prvSetEDFPriorities( int debug_prnt );
void prvSetHVDFPriorities( int debug_prnt );

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

#if( schedUSE_SCHEDULER_TASK )
	static TickType_t xSchedulerWakeCounter = 0;
	static TaskHandle_t xSchedulerHandle = NULL;
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
		//Make sure the index is valid before checking that the element is in use so we dont crash
		configASSERT( xIndex >= 0 && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS);
		//Make sure the element is in use before changing it for no reason
		configASSERT(xTCBArray[ xIndex ].xInUse == pdTRUE);

		//Set the element as not in use and decrement the number of tasks
		xTCBArray[ xIndex ].xInUse = pdFALSE;
		xTaskCounter--;
	}
	
#endif /* schedUSE_TCB_ARRAY */

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
	
	
	pxNewTCB->xRelativeDeadline = xDeadlineTick;
	pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
	pxNewTCB->xWorkIsDone = pdFALSE;
    
	#if( schedUSE_TCB_ARRAY == 1 )
		pxNewTCB->xInUse = pdTRUE;
	#endif /* schedUSE_TCB_ARRAY */
	
	/** Do any other member init here if needed **/
	pxNewTCB->xPriorityIsSet = pdFALSE;
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
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
		sprintf(strBuf, "Creating task: %s with C = %d, T = %d, D = %d", pxNewTCB->pcName, pxNewTCB->xMaxExecTime, pxNewTCB->xPeriod, pxNewTCB->xRelativeDeadline);
		
		#if ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF )
			Serial.print(strBuf);
			Serial.print(" V = ");
			Serial.println(pxNewTCB->xValue);
		#else
			Serial.println(strBuf);
		#endif
	#endif
	
	/** Track the deadline for future reference. This will be used to determine when the 
	 ** scheduler is woken up
	 **/
	wake_scheduler_logic(NEW_MULTIPLE, xDeadlineTick);
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle )
{
	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t indexToRemove = prvGetTCBIndexFromHandle( xTaskHandle );
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

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	/* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
void prvSetFixedPriorities( void )
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

#if ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
/** TODO: Implement this for DPS algos **/
void prvSetEDFPriorities( int debug_prnt ) {
	
	#if PRINT_HEADER
		static int first = 1;
		if (debug_prnt && first) {
			Serial.println("\nSetting initial priorities (EDF)...");
		}
	#endif /* PRINT_HEADER */	
	
	UBaseType_t xIndex;
	TickType_t furthest_dl;
	UBaseType_t idx_tracker;
	UBaseType_t xIndex_2 = 0;
	UBaseType_t prio = 1;
	
	/** Set all the xPriorityIsSet's to pdFALSE **/
	for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
		xTCBArray[xIndex].xPriorityIsSet = pdFALSE;
	}
	
	/** Now iterate through the tasks and re-assign priorities **/
	for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
		
		furthest_dl = xTaskGetTickCount(); /* Set to closest dl possible initially */
		idx_tracker = 0;
		
		/** Searching for the furthest deadline **/
		for (xIndex_2 = 0; xIndex_2 < xTaskCounter; xIndex_2++) {
			
			/** Skip over any task who's priority is already set **/
			if (xTCBArray[xIndex_2].xPriorityIsSet == pdFALSE) {
				if (xTCBArray[xIndex_2].xAbsoluteDeadline > furthest_dl) {
					idx_tracker = xIndex_2;
					furthest_dl = xTCBArray[xIndex_2].xAbsoluteDeadline;
				}
			}
		}
		
		/** Setting the lowest priority **/
		xTCBArray[idx_tracker].uxPriority = prio;
		xTCBArray[idx_tracker].xPriorityIsSet = pdTRUE;
		prio++;
	}
	
	/** At first in testing the scheduler was not respecting the new priorities assigned
	 ** to the tasks by the EDF algorithm. The below code forces it to respect the EDF
	 ** priorities
	 **/
	
	if (!first) {
		for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
			switch (xIndex) {
				case 0:
					vTaskPrioritySet(xHandle1, xTCBArray[xIndex].uxPriority);
					break;
				case 1:
					vTaskPrioritySet(xHandle2, xTCBArray[xIndex].uxPriority);
					break;
				case 2:
					vTaskPrioritySet(xHandle3, xTCBArray[xIndex].uxPriority);
					break;
				case 3:
					vTaskPrioritySet(xHandle4, xTCBArray[xIndex].uxPriority);
					break;
			}
		}		
	}
	
	/** Printing out each tasks priority after this initial set... **/
	#if (PRINT_HEADER)
		if (debug_prnt) {
			for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
				Serial.print("\t(Sch) Set: ");
				Serial.print(xTCBArray[xIndex].pcName);
				Serial.print(" priority: ");
				Serial.println(xTCBArray[xIndex].uxPriority);
			}
			if (first) {
				Serial.println("---------------END OF HEADER-----------------");
				first = 0;
			}
		}
	#endif /* PRINT_HEADER */
}
#endif


#if ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF )
void prvSetHVDFPriorities( int debug_prnt ) {
	
	#if PRINT_HEADER
		static int first = 1;
		if (debug_prnt && first) {
			Serial.println("\nSetting initial priorities (HVDF)...");
		}
	#endif /* PRINT_HEADER */	
	
	UBaseType_t xIndex;
	float lowest_vd;
	UBaseType_t idx_tracker;
	UBaseType_t xIndex_2 = 0;
	UBaseType_t prio = 1;
	
	/** Set all the xPriorityIsSet's to pdFALSE **/
	for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
		xTCBArray[xIndex].xPriorityIsSet = pdFALSE;
	}
	
	/** Now iterate through the tasks and re-assign priorities **/
	for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
		
		lowest_vd = 100000; /* Set to very large value */
		idx_tracker = 0;
		
		/** Searching for the lowest value density **/
		for (xIndex_2 = 0; xIndex_2 < xTaskCounter; xIndex_2++) {
			
			/** Skip over any task who's priority is already set **/
			if (xTCBArray[xIndex_2].xPriorityIsSet == pdFALSE) {
				TickType_t tmp_time_remaining = xTCBArray[xIndex_2].xMaxExecTime - xTCBArray[xIndex_2].xExecTime;
				float tmpVD = ( (float)xTCBArray[xIndex_2].xValue / (float)tmp_time_remaining );
				xTCBArray[xIndex_2].xValueDensity = tmpVD;
				if (tmpVD < lowest_vd) {
					idx_tracker = xIndex_2;
					lowest_vd = tmpVD;
				}
			}
		}
		
		/** Setting the lowest priority **/
		xTCBArray[idx_tracker].uxPriority = prio;
		xTCBArray[idx_tracker].xPriorityIsSet = pdTRUE;
		prio++;
	}
	
	/** At first in testing the scheduler was not respecting the new priorities assigned
	 ** to the tasks by the EDF algorithm. The below code forces it to respect the EDF
	 ** priorities
	 **/
	
	if (!first) {
		for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
			switch (xIndex) {
				case 0:
					vTaskPrioritySet(xHandle1, xTCBArray[xIndex].uxPriority);
					break;
				case 1:
					vTaskPrioritySet(xHandle2, xTCBArray[xIndex].uxPriority);
					break;
				case 2:
					vTaskPrioritySet(xHandle3, xTCBArray[xIndex].uxPriority);
					break;
				case 3:
					vTaskPrioritySet(xHandle4, xTCBArray[xIndex].uxPriority);
					break;
			}
		}
	}
	
	/** Printing out each tasks priority after this initial set... **/
		if (debug_prnt) {
			#if (PRINT_SCHEDULER_DEBUG)
				for (xIndex = 0; xIndex < xTaskCounter; xIndex++) {
					Serial.print("\t(Sch) Set: ");
					Serial.print(xTCBArray[xIndex].pcName);
					Serial.print(" prio: ");
					Serial.print(xTCBArray[xIndex].uxPriority);
					Serial.print(" VD: ");
					Serial.println(xTCBArray[xIndex].xValueDensity);
				}
			#endif
			
			#if (PRINT_HEADER)
				if (first) {
					Serial.println("---------------END OF HEADER-----------------");
					first = 0;
				}
			#endif
		}
}
#endif

static void prvSetInitialDeadlines( void ) {
	
	BaseType_t xIter;
	TickType_t curTime = xTaskGetTickCount();
	
	for( xIter = 0; xIter < xTaskCounter; xIter++ )	{
		xTCBArray[xIter].xAbsoluteDeadline = curTime + xTCBArray[xIter].xRelativeDeadline;
	}
}

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
		BaseType_t xReturnValue = xTaskCreate((TaskFunction_t)prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle);
		
		if( pdPASS == xReturnValue )
		{
	
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
		
		pxTCB->xLastWakeTime = 0;
		pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{ 		
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
		 
		/** We only want to call the below hook when we actually miss a deadline **/
		if (xTickCount >= pxTCB->xAbsoluteDeadline && pxTCB->xWorkIsDone == pdFALSE && pxTCB->xExecutedOnce == pdTRUE) {
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
		Serial.print("Time exceeded hook, ");
		Serial.print(pxCurrentTask->pcName);
		Serial.print(",  ExecTime= ");
		Serial.println(pxCurrentTask->xExecTime);
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
		//No need to check if the task is not in use (i.e. it has been "deleted" from the array)
		if(pxTCB->xInUse == pdTRUE)
		{
			#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )						
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
				#if PRINT_SCHEDULER_DEBUG
					sprintf(msgBuf, "\t(Sch) SUSPENDING: %s", pxTCB->pcName);
					Serial.println(msgBuf);
				#endif
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
	
	/** This function will either save-off a new multiple value at which
	 ** the scheduler should wake, or will determine whether the scheduler should
	 ** wake up given a current tick count. Note: this function has space for 
	 ** 10 "critical" frequencies at which to wake up the scheduler
	 **/
	int wake_scheduler_logic(sch_wake_args_t ftype, TickType_t arg) {
		static TickType_t freq_list[10];
		static int num_crit_pts = 0;
		int cnt = 0;
		switch (ftype) {
			case NEW_MULTIPLE:
					/** We got a new critical point to keep track of **/
					freq_list[num_crit_pts] = arg;
					num_crit_pts++;
				break;
				
			case CHECK_TIME:
					for (cnt = 0; cnt < num_crit_pts; cnt++) {
						if (arg % freq_list[cnt] == 0) {
							return 1; /** We need to wake the scheduler **/
						}
					}
				break;
		}
		return 0; /** By default do not wake scheduler **/
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
						
						prvSchedulerCheckTimingError( xTickCount, pxTCB );
					}
				#endif /* schedUSE_TCB_ARRAY */
			
			#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
			
			#if ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
				prvSetEDFPriorities(1);
			#endif
						
			#if ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF )
				prvSetHVDFPriorities(1);
			#endif
			
			#if (AUGMENT_SCHEDULER == 1)
				Serial.println("DELAYING THE SCHEDULER!");
				delay(.05);
			#endif
			
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
			
			/** This check should eventually be moved to the scheduler function **/
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
			/** Determine if the scheduler needs to be woken up, which is only
			 ** true if we're currently at a "critical point"
			 **/
			xSchedulerWakeCounter++;
			if (wake_scheduler_logic(CHECK_TIME, xSchedulerWakeCounter)) {
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
		#elif (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
			Serial.println("Init scheduler (EDF)\n");
		#endif
		
		/** Note: When I tested this, I found that tics occur at a frequency of 62 hz **/
		/** TODO: 62 hz seems really slow. I'd eventually like to make this much faster **/
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
	/** Set initial deadlines */
	prvSetInitialDeadlines();
	
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
		prvSetFixedPriorities();
	#elif ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
		prvSetEDFPriorities(1);
	#elif ( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF )
		prvSetHVDFPriorities(1);
	#endif /* schedSCHEDULING_POLICY */

	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();
	  
	xSystemStartTime = xTaskGetTickCount();
	
	vTaskStartScheduler();
}
