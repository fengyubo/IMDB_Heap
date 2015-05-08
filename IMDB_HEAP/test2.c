#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
//#include <whitedb/dbapi.h>
#include "dbapi.h"


//You can adjust how many things are allocated
#define TIMES 50

//If you want to make times bigger than 100, remove the call to qsort and do something else.
//Then remove this check.
#if TIMES >= 1000
	#error "TIMES is too big, qsort will switch to mergesort which requires a temporary malloc()ed array"
#endif

//Do not modify below here
struct tree {
	int data;
	struct tree *left;
	struct tree *right;
};

void* db;
int trackd=1;

void freetree(struct tree *root)
{
	if(root->left != NULL){
//		printf("left %d \n",trackd++);
		freetree(root->left);
	}
	if(root->right != NULL){
//		printf("right %d \n",trackd++);
		freetree(root->right);
//		printf("leaveleft %d\n",trackd++);
	}
	printf("point %d\n",trackd++);
	dbfree(db,root);
	//free(root);
}

void randominsert(struct tree *head, struct tree *new)
{
	int way;
	struct tree *curr, *prev;
	prev = NULL;
	curr = head;
	//srand((unsigned)time(0));
	while(curr != NULL)
	{
		prev = curr;
		way = rand()%2;
		if(way == 0)
		{
			curr = curr->left;
		}
		else
		{
			curr = curr->right;
		}
	}
	if(way == 0)
		prev->left = new;
	else
		prev->right = new;
}

void printtree(struct tree *head)
{
	struct tree *curr = head;
	if(head == NULL)
		return;

	printtree(curr->left);	
	printtree(curr->right);
	printf("%d\n", curr->data);
}			 

void test1()
{
	int i;
 	
	struct tree *head = (struct tree *)dbmalloc(db,sizeof(struct tree));
	head->data = 0;
	head->left = NULL;
	head->right = NULL;
	trackd = 1;
	for(i=1;i<TIMES;i++)
	{
		struct tree *new = (struct tree *)dbmalloc(db,sizeof(struct tree));
		new->data = i;
		new->left = NULL;
		new->right = NULL;
		randominsert(head, new);
		trackd ++;
	}
	trackd = 1;
	printtree(head);
	freetree(head);
}

int comp(const void *a, const void *b)
{
	return *((int *)a) - *((int *)b);
}

void test2()
{
	int *a;
	int i;

	a = (int *)dbmalloc(db,TIMES * sizeof(int));

	for(i=0;i<TIMES;i++)
	{
		a[i] = rand()%TIMES + 1;
	}

	qsort(a, TIMES, sizeof(int), comp);

	for(i=0;i<TIMES;i++)
	{
		printf("%d\n", a[i]);
	}

	dbfree(db,a);
	
}

int main(int argc, char **argv){
	const char* test_str = "TEST STRING HEAP";
//	void* db;
  	db = wg_attach_database("1000", 2000000);
  	test1();
	wg_delete_database("1000");

  return 0;
}
