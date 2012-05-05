//
//	assignment2.cpp
//
//			Mar 03, 2011
//			Bo Yu

#include<stdio.h>
#include<pthread.h>
#include<sys/time.h>
#include<unistd.h>
#include<iostream>
#include<string>
#include<vector>
#include<queue>
#include<list>
#include<ctime>
#include<cstdlib> // Random numbers
#include<algorithm>
using namespace std;

//
//	Preprocessor
//
//#define DEBUG_PRODUCTS_INFO
#define DEBUG_TERMINATE_THREADS
#define DEBUG_RR
//#define DEBUG_PRODUCTS_INFO

//
//	Enumeration.
//
enum ProductState { Empty, Hold };
enum SchedulingAlgorithm { FCFS, RR };

//
//	Structures.
//
// This structure represent a product.
struct Product
{
	// A unique product id.
	int nId;
	// A time-stamp of when the product was generated.
	timeval tvGenerateTime;
	// The "life" of product, which is a positive integer number that is randomly generated.
	// The value should be capped at 1024 numbers by calling random in the following manner:
	// rand()%1024
	int nLife;
};
// This structure represent an item in the product queue.
struct ProductQueueItem
{
	Product * pProduct;
	ProductState state;
};

//	This structure is responsible for collect producer thread information.
struct ProducerThreadInfo
{
	pthread_t threadId;	//	Thread id.
	int productNum;		//	How many products are produced.
};

//	This structure is responsible for collect consumer thread information.
struct ConsumerThreadInfo
{
	pthread_t threadId;	//	Thread id.
	int productNum;		//	How many products are consumed.
};

//
//	Global variables.
//
#define PRODUCER_THREAD_COUNT 4
#define CONSUMER_THREAD_COUNT 4
#define PRODUCT_TOTAL_SIZE 100
#define PRODUCT_QUEUE_SIZE 10
#define QUANTUM 100
#define SEED 5

//	Scheduling algorithm.
SchedulingAlgorithm g_enumSchedulingAlgorithm = FCFS;
//	Value of quantum used for round-robin scheduling.
int g_iQuantum = QUANTUM;

// 	Producer threads
pthread_t 	g_pidProducerThreadsArray [ PRODUCER_THREAD_COUNT ];
// 	Producer thread attributes
pthread_attr_t 	g_pattrProducerThreadsAttrArray [ PRODUCER_THREAD_COUNT ];
//	Collect producer thread info
vector<ProducerThreadInfo> g_vecProducerThreadInfo ( PRODUCER_THREAD_COUNT );

// 	Consumer threads
pthread_t 	g_pidConsumerThreadsArray [ CONSUMER_THREAD_COUNT ];
// 	Consumer thread attributes
pthread_attr_t 	g_pattrConsumerThreadsAttrArray [ CONSUMER_THREAD_COUNT ];
//	Collect consumer thread info
vector<ConsumerThreadInfo> g_vecConsumerThreadInfo ( CONSUMER_THREAD_COUNT );

// 	Condition variables
pthread_cond_t g_condNotFull = PTHREAD_COND_INITIALIZER;
pthread_cond_t g_condNotEmpty = PTHREAD_COND_INITIALIZER;
// 	Mute lock
pthread_mutex_t g_mutexLock = PTHREAD_MUTEX_INITIALIZER;

// 	Product array.
vector<Product *> g_vecProductPtrs (PRODUCT_TOTAL_SIZE);
//	How many products have produced.
int g_nProducedProductNum = 0;
// 	Product queue served for producers and consumers.
list<ProductQueueItem> g_listProductQueue (PRODUCT_QUEUE_SIZE);
//	How many products have produced.
int g_nConsumedProductNum = 0;
// 	Get next product.
vector<Product *>::iterator g_iterNextProduct;

//
//	Time measure-related variables and functions.
//

//	This structure records the begin and end point of a product's execution time.
struct ProductExecutionTime
{
	timeval tvBeginTime;
	timeval tvEndTime;
	bool bIsMarkBeginTime;
	bool bIsMarkEndTime;
};
vector<ProductExecutionTime> g_vecProductExecutionTime (PRODUCT_TOTAL_SIZE);
void StartMeasureProductExecutionTime ( int nId )
{
	if ( g_vecProductExecutionTime[nId-1].bIsMarkBeginTime == false )
	{
		gettimeofday ( &g_vecProductExecutionTime[nId-1].tvBeginTime, NULL );
		g_vecProductExecutionTime[nId-1].bIsMarkBeginTime = true;
	}
}
void StopMeasureProductExecutionTime ( int nId )
{
	if ( g_vecProductExecutionTime[nId-1].bIsMarkEndTime == false )
	{
		gettimeofday ( &g_vecProductExecutionTime[nId-1].tvEndTime, NULL );
		g_vecProductExecutionTime[nId-1].bIsMarkEndTime = true;
	}
}
void InitProductExecutionTimeVector()
{
	for ( unsigned int i = 0; i < g_vecProductExecutionTime.capacity(); i++ )
	{
		g_vecProductExecutionTime[i].bIsMarkBeginTime = false;
		g_vecProductExecutionTime[i].bIsMarkEndTime = false;
	}
}

//	This structure records the begin and end point of a product's turn-around time.
struct ProductTurnAroundTime
{
	timeval tvBeginTime;
	timeval tvEndTime;
	bool bIsMarkBeginTime;
	bool bIsMarkEndTime;
};
vector<ProductTurnAroundTime> g_vecProductTurnAroundTime (PRODUCT_TOTAL_SIZE);
void StartMeasureProductTurnAroundTime ( int nId )
{
	if ( g_vecProductTurnAroundTime[nId-1].bIsMarkBeginTime == false )
	{
		gettimeofday ( &g_vecProductTurnAroundTime[nId-1].tvBeginTime, NULL );
		g_vecProductTurnAroundTime[nId-1].bIsMarkBeginTime = true;
	}
}
void StopMeasureProductTurnAroundTime ( int nId )
{
	if ( g_vecProductTurnAroundTime[nId-1].bIsMarkEndTime == false )
	{
		gettimeofday ( &g_vecProductTurnAroundTime[nId-1].tvEndTime, NULL );
		g_vecProductTurnAroundTime[nId-1].bIsMarkEndTime = true;
	}
}
void InitProductTurnAroundTimeVector()
{
	for ( unsigned int i = 0; i < g_vecProductTurnAroundTime.capacity(); i++ )
	{
		g_vecProductTurnAroundTime[i].bIsMarkEndTime = false;
		g_vecProductTurnAroundTime[i].bIsMarkBeginTime = false;
	}
}

//	This structure records the begin and end point for a product's wait time.
struct ProductWaitTime
{
	timeval tvBeginTime;
	timeval tvEndTime;
	bool bIsMarkEndTime;
	bool bIsMarkBeginTime;
};
vector<ProductWaitTime> g_vecProductWaitTime (PRODUCT_TOTAL_SIZE);
void StartMeasureProductWaitTime ( int nId )
{
	if ( g_vecProductWaitTime[nId-1].bIsMarkBeginTime == false )
	{
		gettimeofday ( &g_vecProductWaitTime[nId-1].tvBeginTime, NULL );
		g_vecProductWaitTime[nId-1].bIsMarkBeginTime = true;
		//cout << "Get product " << nId << " wait time at begin." << endl;
	}
}
void StopMeasureProductWaitTime ( int nId )
{
	if ( g_vecProductWaitTime[nId-1].bIsMarkEndTime == false )
	{
		gettimeofday ( &g_vecProductWaitTime[nId-1].tvEndTime, NULL );
		g_vecProductWaitTime[nId-1].bIsMarkEndTime = true;
		//cout << "Get product " << nId << " wait time at end." << endl;
	}
}
void InitProductWaitTimeVector()
{
	for ( unsigned int i = 0; i < g_vecProductWaitTime.capacity(); i++ )
	{
		g_vecProductWaitTime[i].bIsMarkEndTime = false;
		g_vecProductWaitTime[i].bIsMarkBeginTime = false;
	}
}

//	This structure records the begin and end point for processing all products.
//	From the first time of submission of a process to the last time of completion of a process.
struct TotalTimeForProcessingAllProducts
{
	timeval tvBeginTime;
	timeval tvEndTime;
};
TotalTimeForProcessingAllProducts g_totalTimeForProcessingAllProducts;
void StartMeasureTotalTimeForProcessingAllProducts ()
{
	gettimeofday ( &g_totalTimeForProcessingAllProducts.tvBeginTime, NULL );
}
void StopMeasureTotalTimeForProcessingAllProducts ()
{
	gettimeofday ( &g_totalTimeForProcessingAllProducts.tvEndTime, NULL );
}

struct ProducerThroughputTime
{
	timeval tvBeginTime;
	timeval tvEndTime;
	bool bIsMarkBeginTime;
	bool bIsMarkEndTime;
};
ProducerThroughputTime g_producerThroughputTime;
void StartMeasureProducerThroughputTime ()
{
	if ( g_producerThroughputTime.bIsMarkBeginTime == false )
	{
		gettimeofday ( &g_producerThroughputTime.tvBeginTime, NULL );
		g_producerThroughputTime.bIsMarkBeginTime = true;
	}
}
void StopMeasureProducerThroughputTime ()
{
	if ( g_producerThroughputTime.bIsMarkEndTime == false )
	{
		gettimeofday ( &g_producerThroughputTime.tvEndTime, NULL );
		g_producerThroughputTime.bIsMarkEndTime = true;
	}
}
void InitProducerThroughputTime()
{
		g_producerThroughputTime.bIsMarkEndTime = false;
		g_producerThroughputTime.bIsMarkBeginTime = false;
}

struct ConsumerThroughputTime
{
	timeval tvBeginTime;
	timeval tvEndTime;
	bool bIsMarkBeginTime;
	bool bIsMarkEndTime;
};
ConsumerThroughputTime g_consumerThroughputTime;
void StartMeasureConsumerThroughputTime ()
{
	if ( g_consumerThroughputTime.bIsMarkBeginTime == false )
	{
		gettimeofday ( &g_consumerThroughputTime.tvBeginTime, NULL );
		g_consumerThroughputTime.bIsMarkBeginTime = true;
	}
}
void StopMeasureConsumerThroughputTime ()
{
	if ( g_consumerThroughputTime.bIsMarkEndTime == false )
	{
		gettimeofday ( &g_consumerThroughputTime.tvEndTime, NULL );
		g_consumerThroughputTime.bIsMarkEndTime = true;
	}
}
void InitConsumerThroughputTime()
{
		g_consumerThroughputTime.bIsMarkEndTime = false;
		g_consumerThroughputTime.bIsMarkBeginTime = false;
}

double GetTimeInSeconds ( timeval tvBegin, timeval tvEnd )
{
	long sec, usec;
	sec = tvEnd.tv_sec - tvBegin.tv_sec;
	usec = tvEnd.tv_usec - tvBegin.tv_usec;
	return sec + 1e-6*usec;
}

void PrintExperimentalInfo ()
{
	//	>>>>>>Track the total time for processing all the products.
	StopMeasureTotalTimeForProcessingAllProducts ();
	cout << endl << "<<<<<<<<<<<<<<<Experimentaion Infomation.<<<<<<<<<<<<<<<" << endl;

	//	Total time for processing all products.
	cout << "Total time for processing all products = "
		 << GetTimeInSeconds(g_totalTimeForProcessingAllProducts.tvBeginTime,g_totalTimeForProcessingAllProducts.tvEndTime)
		 << endl;

	//	Produced products information.
	cout << "Total produced products = " << g_nProducedProductNum << endl;
	for ( unsigned int i = 0; i < g_vecProducerThreadInfo.capacity(); i++ )
	{
		cout << "[Thread " << g_vecProducerThreadInfo[i].threadId << " ] has produced " << g_vecProducerThreadInfo[i].productNum << endl;
	}

	//	Consuemd products information.
	cout << "Total consumed products = " << g_nConsumedProductNum << endl;
	for ( unsigned int i = 0; i < g_vecConsumerThreadInfo.capacity(); i++ )
	{
		cout << "[Thread " << g_vecConsumerThreadInfo[i].threadId << " ] has consumed " << g_vecConsumerThreadInfo[i].productNum << endl;
	}

	//	Execution time for each product.
	double totalExecutionTime = 0.0;
	for ( unsigned int i = 0; i < g_vecProductExecutionTime.capacity(); i++ )
	{
		double executionTime = GetTimeInSeconds ( g_vecProductExecutionTime[i].tvBeginTime, g_vecProductExecutionTime[i].tvEndTime );
//		cout << "Product " << i+1 << "Execution Time = " << executionTime << endl;
		totalExecutionTime += executionTime;
	}
	cout << "Total Execution Time for all products ( " << PRODUCT_TOTAL_SIZE << " ) = " << totalExecutionTime << endl;
	cout << "Mean of average Execution time for each product = " << totalExecutionTime / PRODUCT_TOTAL_SIZE << endl;

	//	Wait time for each product.
	double totalWaitTime = 0.0;
	double maxWaitTime = 0.0;
	double minWaitTime = 0.0;
	for ( unsigned int i = 0; i < g_vecProductWaitTime.capacity(); i++ )
	{
		double waitTime = GetTimeInSeconds ( g_vecProductWaitTime[i].tvBeginTime, g_vecProductWaitTime[i].tvEndTime );
		cout << "Product " << i+1 << "Wait Time = " << waitTime << endl;
		totalWaitTime += waitTime;
	}
	cout << "Total Wait Time for all products ( " << PRODUCT_TOTAL_SIZE << " ) = " << totalWaitTime << endl;
	cout << "Mean of average Wait Time for each product = " << totalWaitTime / PRODUCT_TOTAL_SIZE << endl;

	//	Turn-Around time for each product.
	double totalTurnAroundTime = 0.0;
	for ( unsigned int i = 0; i < g_vecProductTurnAroundTime.capacity(); i++ )
	{
		double turnAroundTime = GetTimeInSeconds ( g_vecProductTurnAroundTime[i].tvBeginTime, g_vecProductTurnAroundTime[i].tvEndTime );
		cout << "Product " << i+1 << "Turn-Around Time = " << turnAroundTime << endl;
		totalTurnAroundTime += turnAroundTime;
	}
	cout << endl<< endl << "Total Turn-Around Time for all products ( " << PRODUCT_TOTAL_SIZE << " ) = " << totalTurnAroundTime << endl;
	cout << "Mean of average Turn-Around Time for each product = " << totalTurnAroundTime / PRODUCT_TOTAL_SIZE << endl;

	//	Producer throughput.
	double timeForProduceProducts;
	timeForProduceProducts = GetTimeInSeconds (g_producerThroughputTime.tvBeginTime, g_producerThroughputTime.tvEndTime);
	double minutes = timeForProduceProducts / 60;
	cout << "Producer throughput = " << PRODUCT_TOTAL_SIZE / minutes << endl;

	//	Consumer throughput.
	double timeForConsumeProducts;
	timeForConsumeProducts = GetTimeInSeconds ( g_consumerThroughputTime.tvBeginTime, g_consumerThroughputTime.tvEndTime );
	double mins = timeForConsumeProducts / 60;
	cout << "Consumer throughput = " << PRODUCT_TOTAL_SIZE / mins << endl;
}

//
// Function prototypes for products.
//
// Initialize the products.
void InitProducts();
// Get an exist product to consume.
Product * GetNewProduct ();
//	Print out the products information.
void PrintProducts();

//
// Function prototypes for product queue.
//
// Initialize the product queue.
void InitProductQueue ();
//	Tests if the queue is full.
bool IsFull ();
//	Tests if the queue is empty.
bool IsEmpty ();
//	Print out the details of the queue items.
void PrintProductQueueItems ();

//
//	Utility functions.
//
int Fibonacci ( int number );

//
// Thread-related functions.
//
void * ProducerThreadFunc ( void * argu );
void * ConsumerThreadFunc ( void * argu );
void CreateProducerThreads ();
void CreateConsumerThreads ();
void TerminateProducerThreads ();
void TerminateConsumerThreads ();
void ProduceProduct ();
void ConsumeProduct ();

/////////////////////////////////////////////////////////////////////////////////////

void * func(void * argu)
{
	pthread_t * pTid = (pthread_t*) argu;
	cout << "pid = " << *pTid << endl;
}
//
//	Main function.
//
int main()
{
	//	Initialization.
	InitProductWaitTimeVector();
	InitProductExecutionTimeVector();
	InitProductTurnAroundTimeVector();
	InitProducerThroughputTime();
	InitConsumerThroughputTime();

	// 	Initialize the products.
	InitProducts ();

	//	Initialize the product queue.
	InitProductQueue();

#ifdef DEBUG_PRODUCTS_INFO
	//	Print all the products.
	PrintProducts();
	//	Print all the product queue items.
	PrintProductQueueItems ();
#endif

	// 	Create all the producer and consumer threads.
	CreateProducerThreads ();
	CreateConsumerThreads ();

	//	Terminate all the producer and consumer threads.
	TerminateProducerThreads ();
	TerminateConsumerThreads ();

}

//
//	Pick up a product from product vector and insert it into the product queue.
//
void ProduceProduct ()
{
	//	Require access for writing.
	pthread_mutex_lock ( & g_mutexLock );
	//	Product queue is full?
	while ( IsFull() )
	{
		//	Need to wait for a consumer to empty a slot.
		pthread_cond_wait ( &g_condNotFull, &g_mutexLock );
	}

	//	Enter critical region.////////////////////////////////////////////

	//
	// Insert item
	//
	// 1. Pick up a product from product vector
	Product * pNewProduct = NULL;
	if ( !g_vecProductPtrs.empty() )
	{
		pNewProduct = GetNewProduct ();
	}
	// 	Total products have been produced, then all producer threads
	//	must terminate producing and return.
	else
	{
		//	...

		pthread_mutex_unlock ( & g_mutexLock );
		pthread_cond_signal ( & g_condNotEmpty );
		return;
	}

	//	2. Insert it into the product queue.
	list<ProductQueueItem>::iterator iter;
	for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
	{
		//	Find an empty slot and insert the product into this slot.
		if ( (*iter).state == Empty )
		{
			(*iter).pProduct = pNewProduct;
			(*iter).state = Hold;

			if ( (*iter).pProduct->nId == 1 )
			{
				//	<<<<<<Track the total time for processing all the products.
				StartMeasureTotalTimeForProcessingAllProducts ();
				//	<<<<<<Track the throughput for producer.
				StartMeasureProducerThroughputTime();
			}
			if ((*iter).pProduct->nId == PRODUCT_TOTAL_SIZE)
			{
				//	>>>>>>Track the throughput for producer.
				StopMeasureProducerThroughputTime();
			}
			//	<<<<<<Track the turn-around start time of this product.
			StartMeasureProductTurnAroundTime ( (*iter).pProduct->nId );
			//	<<<<<<Track the product's start time for wait.
			StartMeasureProductWaitTime ( (*iter).pProduct->nId );

			break;
		}
	}

	//  3. Print the product information.
	cout << "[Thread "<< pthread_self() << " ] Product id = " << pNewProduct->nId << " has produced." << endl;

	//	4. Record this thread has produced an item into the product queue.
	for ( int i = 0; i < g_vecProducerThreadInfo.capacity(); i++ )
	{
		if ( g_vecProducerThreadInfo[i].threadId == pthread_self() )
		{
			g_vecProducerThreadInfo[i].productNum++;
			break;
		}
	}

	//	5. Update total number of produced products.
	g_nProducedProductNum ++;

	//	6. Sleep 100 milliseconds.
	//	   Note: the input parameter for usleep() is microsecond,
	//	   while you need to let the thread sleep for 100 milliseconds.
	usleep ( 100*1000 );

	//	Exit critical region.////////////////////////////////////////////

	//	No need to keep the lock any longer.
	pthread_mutex_unlock ( & g_mutexLock );
	//	Signal consumer threads that there is an element to consume.
	pthread_cond_signal ( & g_condNotEmpty );
}

//
//	Remove a product from queue.
//
void ConsumeProduct ()
{
	//	Get access to the queue to consume a product.
	pthread_mutex_lock ( & g_mutexLock );
	//	No item to consume?
	while ( IsEmpty() )
	{
		//	Wait until a producer adds a new product into the queue.
		pthread_cond_wait ( & g_condNotEmpty, & g_mutexLock );
	}

	// 	Enter the critical region!!! ////////////////////////////////////////////

	//
	// Consume a product.
	//

	//
	//	Using FCFS algorithm.
	//
	int nProductId;
	if ( g_enumSchedulingAlgorithm == FCFS )
	{
		list<ProductQueueItem>::iterator iter;
		int nProductLife;
		for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
		{
			//	1. Get a item from product queue
			if ( (*iter).state == Hold )
			{
				//	<<<<<<Track the execution start time of this product.
				StartMeasureProductExecutionTime ( (*iter).pProduct->nId );
				//	>>>>>>Track the end time for wait.
				StopMeasureProductWaitTime ( (*iter).pProduct->nId );
				//	2. Marked this item slot as Empty.
				(*iter).state = Empty;
				//	3. delete the product.
				nProductId = (*iter).pProduct->nId;
				nProductLife = (*iter).pProduct->nLife;
				delete (Product*)(*iter).pProduct;
				break;
			}
		}
		//	4. The consumption of a product is simulated by calling
		//	the fn(10) function N times.
		for ( int i = 0; i < nProductLife; i++ )
		{
			Fibonacci ( 10 );
		}

		//	>>>>>>Track the execution end time of this product.
		StopMeasureProductExecutionTime ( nProductId );
		//	>>>>>>Track the turn-around end time of this product.
		StopMeasureProductTurnAroundTime ( nProductId );

		//  5. Print out the product id.
		cout << "[Thread " << pthread_self() << "] Product id = " << nProductId << " has consumed." << endl;
		//	6. Record this thread has consumed an item from the product queue.
		for ( int i = 0; i < g_vecConsumerThreadInfo.capacity(); i++ )
		{
			if ( g_vecConsumerThreadInfo[i].threadId == pthread_self() )
			{
				g_vecConsumerThreadInfo[i].productNum++;
				break;
			}
		}
		//	7. Update global number of consumed product.
		g_nConsumedProductNum ++;

	}	// End of FCFS Algorithm.
	//
	//	Using RR algorithm.
	//
	else if ( g_enumSchedulingAlgorithm == RR )
	{
		//	1. Get an item from product queue.
		list<ProductQueueItem>::iterator iter;
		for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
		{
			if ( (*iter).state == Hold )
			{
				//	Iterator catches the item.
				nProductId = (*iter).pProduct->nId;
				break;
			}
		}
		//	2. Check the product's life l>=q.
		if ( (*iter).pProduct->nLife >= g_iQuantum )
		{
			//	<<<<<<Track the execution start time of this product.
			StartMeasureProductExecutionTime ( (*iter).pProduct->nId );
			//	>>>>>>Track the end time for wait.
			StopMeasureProductWaitTime ( (*iter).pProduct->nId );
			//	2.1.1 If yes, the thread updates the product's life l to be l-q.
			(*iter).pProduct->nLife -= g_iQuantum;
			//	2.1.2 Calls Fibonacci function fn(10) q times.
			for ( int i = 0; i < g_iQuantum; i++ )
				Fibonacci ( 10 );
			//	2.1.3 Remove the item from the queue, then put it at the end of the queue.
			ProductQueueItem * pItem = & (*iter);
			g_listProductQueue.erase(iter);
			g_listProductQueue.push_back(*pItem);

			#ifdef DEBUG_RR
			cout << "[Thread " << pthread_self() << "] Product id = " << (*iter).pProduct->nId << " has consumed partially." << endl;
			#endif
		}
		else
		{
			//	<<<<<<Track the execution start time of this product.
			StartMeasureProductExecutionTime ( (*iter).pProduct->nId );
			//	>>>>>>Track the end time for wait.
			StopMeasureProductWaitTime ( (*iter).pProduct->nId );
			//	2.2.1 Otherwise, the thread removes the product from the queue,
			(*iter).state = Empty;
			//...free memory.
			int nProductId = (*iter).pProduct->nId;
			delete (Product *)(*iter).pProduct;
			//	2.2.2 calls Fibonacci function fn(10) 1 times.
			Fibonacci ( 10 );
			//	>>>>>>Track the execution end time of this product.
			StopMeasureProductExecutionTime ( nProductId );
			//	>>>>>>Track the turn-around end time of this product.
			StopMeasureProductTurnAroundTime ( nProductId );
			//  2.2.3 Prints the product's ID.
			cout << "[Thread " << pthread_self() << "] Product id = " << nProductId << " has consumed." << endl;
			//	2.2.4 You should also keep track of the number of products that have been consumed.
			//		  When the number reaches the value of input parameter P3, which is the total
			//		  number of products, all consumer threads terminate consuming and return.
			//	....

			//	2.2.5 Record this thread has consumed an item from the product queue.
			for ( int i = 0; i < g_vecConsumerThreadInfo.capacity(); i++ )
			{
				if ( g_vecConsumerThreadInfo[i].threadId == pthread_self() )
				{
					g_vecConsumerThreadInfo[i].productNum++;
					break;
				}
			}
			//	2.2.6 Update global number of consumed product.
			g_nConsumedProductNum ++;
		}	//	EndOf else

	}	//	End of RR Algorithm.

	//
	//	Print experimental information when all the products consumed.
	//
	//if ( g_nConsumedProductNum == PRODUCT_TOTAL_SIZE )
	//{
	//	PrintExperimentalInfo ();
	//}

	//	<<<<<<Track the consumer throughput time.
	if ( nProductId == 1 )
		StartMeasureConsumerThroughputTime();
	//	>>>>>>Track the consumer throughput time.
	else if ( nProductId == g_nConsumedProductNum )
		StopMeasureConsumerThroughputTime();

	//	Calls usleep() to sleep 100 milliseconds.
	//	So that the caller thread does not hold onto the CPU forever and allow other threads to run.
	usleep ( 100*1000 );


	//  Exit the critical region!!!	////////////////////////////////////////////

	//	No need to keep the lock any longer.
	pthread_mutex_unlock ( & g_mutexLock );
	//	A free slot is now available for producer threads to produce,
	//	wake up a producer thread.
	pthread_cond_signal ( & g_condNotFull );

}	//	EndOf ConsumeProduct()

//
// Reserve the memory for products and initialize their attributes.
//
void InitProducts ()
{
	// Reserve some memory for products.
	//g_vecProductPtrs.reserve ( capacity );

	//	SEED for random number generator.
	srand ( SEED );

	// Generate products and initialize their attributes.
	for ( unsigned int i = 0; i < g_vecProductPtrs.capacity(); i++ )
	{
		// Create a product.
		Product * pNewProduct = new Product;

		//	Push the product into the vector.
		//g_vecProductPtrs.push_back( pNewProduct );
		g_vecProductPtrs[i] = pNewProduct;

		// Assign an unique id.
		g_vecProductPtrs[i]->nId = i+1;

		// Generate an integer number randomly which should be capped at 1024 numbers.
		g_vecProductPtrs[i]->nLife = rand()%1024;

		// Get current time.
		timeval tv;
		gettimeofday ( &tv, NULL );
		g_vecProductPtrs[i]->tvGenerateTime = tv;
	}

	// Initialize the iterator.
	g_iterNextProduct = g_vecProductPtrs.begin();
}

//
// 	Test product vector is not empty first!!
//	Then call this function.
//
Product * GetNewProduct ()
{
	Product * pProduct = ( * g_iterNextProduct );
	g_iterNextProduct ++;
	return pProduct;
}

//
// Slow the detail information of the products.
//
void PrintProducts()
{
	cout << "Print all the products information." << endl;

	for ( unsigned int i = 0; i < g_vecProductPtrs.capacity(); i++ )
	{
		cout << "Product[" << i << "] id = " << g_vecProductPtrs[i]->nId << endl;
		cout << "Product[" << i << "] life = " << g_vecProductPtrs[i]->nLife << endl;
	}

	cout << "IsEmpty()  " << g_vecProductPtrs.empty() << endl;
	cout << "Size() = " << g_vecProductPtrs.size() << endl;
	cout << "Capacity() = " << g_vecProductPtrs.capacity() << endl;
}

//
//	Initialize the product queue items.
//
void InitProductQueue ()
{
	//	Initialize the items.
	list<ProductQueueItem>::iterator iter;
	for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
	{
		(*iter).pProduct = NULL;
		(*iter).state = Empty;
	}
}

//
//	Print all the item detail information.
//
void PrintProductQueueItems ()
{
	cout << "--------------------Print the queue items.-------------------" << endl;

	list<ProductQueueItem>::iterator iter;
	int i = 0;
	for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
	{
		if ( (*iter).state == Empty )
			cout << "ProductQueue [ " << i << " ] state = Empty" << endl;
		else
			cout << "ProductQueue [ " << i << " ] state = Hold" << endl;
		i++;
	}
}

bool IsFull ()
{
	list<ProductQueueItem>::iterator iter;
	for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
	{
		if ( (*iter).state == Empty )
			return false;
	}
	return true;
}

bool IsEmpty ()
{
	list<ProductQueueItem>::iterator iter;
	for ( iter = g_listProductQueue.begin(); iter!=g_listProductQueue.end(); iter++ )
	{
		if ( (*iter).state == Hold )
			return false;
	}
	return true;
}

//
//	This function implement fibonacci algorithm.
//
int Fibonacci ( int number )
{
	if ( (number == 0) || (number == 1) )
	{
		return number;
	}
	else
	{
		return Fibonacci ( number -1 ) + Fibonacci ( number - 2 );
	}
}

//
//	Create producer threads.
//
void CreateProducerThreads()
{
	for ( int i = 0; i < PRODUCER_THREAD_COUNT; i++ )
	{
		pthread_attr_init ( & g_pattrProducerThreadsAttrArray [ i ] );

		pthread_create ( &g_pidProducerThreadsArray [ i ],
			 	 	 	 &g_pattrProducerThreadsAttrArray [ i ],
			 	 	 	 ProducerThreadFunc,
			 	 	 	 NULL );

		//	Initialize the ProducerThreadInfo structure.
		g_vecProducerThreadInfo [i].threadId = g_pidProducerThreadsArray [i];
		g_vecProducerThreadInfo [i].productNum = 0;
	}
}

//
//	Create consumer threads.
//
void CreateConsumerThreads()
{
	for ( int i = 0; i < CONSUMER_THREAD_COUNT; i++ )
	{
		pthread_attr_init ( & g_pattrConsumerThreadsAttrArray [ i ] );
		pthread_create ( &g_pidConsumerThreadsArray [ i ],
			 	 	 	 &g_pattrConsumerThreadsAttrArray [ i ],
			 	 	 	 ConsumerThreadFunc,
			 	 	 	 NULL );
		//	Initialize the ProducerThreadInfo structure.
		g_vecConsumerThreadInfo [i].threadId = g_pidConsumerThreadsArray [i];
		g_vecConsumerThreadInfo [i].productNum = 0;
	}
}

//
//	The newly created producer thread starts running at here.
//
void * ProducerThreadFunc ( void * argu )
{
	while ( 1 )
	{
		ProduceProduct ();
	}
}

//
//	The newly created consumer thread starts running at here.
//
void * ConsumerThreadFunc ( void * argu )
{
	while ( 1 )
	{
		ConsumeProduct ();
	}
}

//
//	Producer threads should stop here.
//
void TerminateProducerThreads ()
{
	#ifdef DEBUG_TERMINATE_THREADS
	cout << "Enter TerminateProducerThreads().\n";
	#endif

	for ( int i = 0; i < PRODUCER_THREAD_COUNT; i++ )
	{
		pthread_join ( g_pidProducerThreadsArray [ i ], NULL );
		cout << "Producer thread ( id = " << g_pidProducerThreadsArray [i]
		     << " ) has terminated.\n";
	}
}

//
//	Consumer threads should stop here.
//
void TerminateConsumerThreads ()
{
	#ifdef DEBUG_TERMINATE_THREADS
	cout << "Enter TerminateConsumerThreads().\n";
	#endif

	for ( int i = 0; i < CONSUMER_THREAD_COUNT; i++ )
	{
		pthread_join ( g_pidConsumerThreadsArray [ i ], NULL );
		cout << "Consumer thread ( id = " << g_pidConsumerThreadsArray [i]
		     << " ) has terminated.\n";
	}
}