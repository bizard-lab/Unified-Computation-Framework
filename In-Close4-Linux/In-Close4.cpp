/* In-Close4 64 bit version. Copyright: Simon Andrews, 2016, Sheffield Hallam University

In-Close CONCEPT MINING (and CONCEPT TREE BUILDING) PROGRAM.

****** LICENSE ****************************************************************************************
In-Close4 is distributed under the MIT 'permissive' open-source software license:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************************************************

Combines breadth and depth processing - closing a concept before generating its decendants. Allows for attribute inheritance - children inherit the intent of parents
and these attribute columns do not neeed to be intersected.
Concepts are closed incrementally.
Cannonicity is tested before closure - this 'partial closure' test makes In-Close faster than other concept miners.

Intent is B, Extent is A. Intents are stored in a linked list tree structure. Extents are stored in a linearised 2-D array.
Rows = objects, columns = attributes.

The context is stored as a bit-array to optimse for RAM and optimise use of cache memory.
This also allows multiple context cells to be processed by single 64-bit operators.

Context columns are sorted physically in acending order of support.
Context rows are sorted physically in order of numerical size to reduce Hamming distance between rows.
Sorting optimises the closure function and physical sorting optimises use of cache memory.

There is min support for A and B. if min support is specified, a reduced context is output removing all the rows/columns not
involved in supported concepts.

INPUT is a Burmeister .cxt file or FIMI .dat file.
OUTPUTS:
1) sorted cxt file (thus can also be used to convert FIMI to Burmeister format).
2) concepts in various format options:
	a) concepts as lists of index numbers of objects and attributes
	b) concepts as lists of names of objects and attributes
	c) where the original data was many-valued, including instances of repeated fields, concepts can be output where an attribute
		consists of the field name and a field value. Repeated fields are captured by using an array of values: "fieldname" : [fieldvalue, fieldvalue, ...]
		An assumption is made that the formal attribute has been formed by concatenating the field name and field value with a delimiter (often the'-' character)
		Thus, for example, if the objects share the attributes "location-Sheffield" and "location-Pitsmoor",
		the corresponding JSON construction will be "location" : ["Sheffield","Pitsmoor"]
	d) a CONCEPT TREE in JSON format A: concepts ouput with child concepts within parent concepts. Can be visualised by uploading ouput concepts.JSON file at http://homepages.shu.ac.uk/~aceslh/fca/fcaTree.html
	e) a CONCEPT TREE in JSON format B: concepts ouput as tree nodes, with node number, parent node number, own attibutes and own objects (suitable for visualising usng D3, for example)
	f) concepts in csv format: each cocnept has one line of cs attributes followed by one line of cs objects.
3) list of <size B> - <#concepts>
4) list of <size A> - <#concepts>
5) if min support is specified, outputs reduced cxt file (using only concepts that satisfy min support).

Cross Platform Port works for Intel, Cray, GNU compilers for Linux, Intel, GNU compilers on MacOS
and Intel, Microsoft compilers for Windows

Last Update : June 2017
*/


#include "hr_time.h"		//for timing
#include "crossplatform.h"
#include <fstream>			//for file IO
#include <iostream>			//for cin >> and cout <<
#include <string.h>			//for strings
#include <vector>
#include <sstream>
#include <algorithm>
using namespace std;

//MAX VALUES:
#define MAX_CONS 23000000	//max number of concepts
#define MAX_COLS 5000		//max number of attributes
#define MAX_ROWS 200000		//max number of objects
#define MAX_FOR_B 40000000	//memory for storing intents
#define MAX_FOR_A 300000000//memory for storing extents
//Change these in the as required and available RAM allows. Current values are for standard 64 bit Windows PC with 8GB RAM

//Values for 4gb ram
//#define MAX_CONS 250000		//max number of concepts
//#define MAX_COLS 5000			//max number of attributes
//#define MAX_ROWS 350000		//max number of objects
//#define MAX_FOR_B 35000000	//memory for storing intents
//#define MAX_FOR_A 50000000	//memory for storing extents

ALIGNED_VECTOR_TYPE(**contextTemp);	//temporary context matrix for input of cxt file and physical sorting of columns
ALIGNED_VECTOR_TYPE(**context);		//the context matrix used for concept mining. 


//the bit-wise columns in 'contextTemp' are transposed into bit-wise rows in 'context' for more efficient use of cache memory
int mArray;					//size of bit-wise columns in temp context
int nArray;					//size of bit-wise rows in context

int colOriginal[MAX_COLS];		//maps sorted columns to original order
int colSup[MAX_COLS];			//column support (for sorting and skipping empty columns)
int rowOriginal[MAX_ROWS];		//maps ham-sorted rows to original order

int n;      //no of attributes {0,1,...,n-1}
int m;		//no of objects    {0,1,...,m-1}

/************************ linked list form of B[MAX_CONS][n] ******************************************/
short int* B;							//pointer to intents (tree in linear array) - memory allocated at start of main
short int sizeBnode[MAX_CONS];			//the no. of attributes at a node (concept labels)
short int * startB[MAX_CONS];			//pointers to start of intents
int nodeParent[MAX_CONS];				//links to parent node in tree
VECTOR_TYPE Bparent[MAX_COLS/64 + 1];	//parent intent in Boolean form (attributes currently involved)
short int sizeB[MAX_CONS];				//intent sizes (calculated after con gen for analysis purposes)
short int * bptr;					//initialise B pointer to start of B

/************************ linear form of A[MAX_CONS][m] ***************************************/

int* A;									//pointer to extents - memory is allocated at start of main
int* startA[MAX_CONS];					//pointers to start of extents
int sizeA[MAX_CONS];					//extent sizes (calculated after con gen for analysis purposes)
int sizeOwnA[MAX_CONS];					//size of extent of own objects for each tree node
int percentA[MAX_CONS];					//percentage of obs in extent vs overvall number of objects (for output purposes only)

int childrenof[MAX_FOR_B];			//children (sub-concepts) of each concept. Linearized childrenof[CONCEPTS][CHILDREN]
int numchildrenof[MAX_CONS];		//number of children of each concept
int* startofchildren[MAX_CONS];		//pointers to start of child list in childrenof
int* cptr;							//pointer to childrenof

int highc = 1;			//highest concept number
int minIn = 0;			//minimum size of intent (min support)
int minEx = 0;			//minimum size of extent (min support)
int startCol = 0;		//starting column for iteration (to skip empty cols)
int numcons = 0;		//number of concepts
char **anames;			//names of attributes
char **onames;			//names of objects
//char fname[100];		//context file name
string fname;
bool FIMI = false;

CStopWatch sInner, sSort; //declare timers

string Jfields[MAX_COLS/3];	     //strings for many valued attribute output
string Jvalues[MAX_COLS/3][100]; //100 is max number of different values

//__int64 inters = 0; //instrumentation to count number of intersections carried out
//__int64 test = 0;   //instrumentation to count number of canonicity tests carried out

int main()
{
	A = new int[MAX_FOR_A];
	B = new short int[MAX_FOR_B];
	bptr = B;
	cptr = childrenof;

	void InClose   (const int c, const int y, VECTOR_TYPE *Bparent);	//incremental concept closure functions
	void InCloseMin(const int c, const int y, VECTOR_TYPE *Bparent); //same as InClose() but with min support for A.
	void InCloseDendo(const int c, const int y, const VECTOR_TYPE *Bparent); //version to record child concepts for dendogram tree output

	void sortRows();				//sort rows on binary value to reduce Hamming distance
	void sortColumns();				//sort columns in ascending order of support
	void sortColumnsR();			//sort columns in descending order of support (for bushy trees)
	void calcAandBsizes();			//calculate sizes of extents and intents
	void cxtFileInput();			//input context in Burmeister format
	void datFileInput();			//input context from dat (FIMI) format
	void outputConcepts();			//output concepts using at and ob index numbers
	void outputConceptsNames();		//output concepts using at and ob names
	void outputNoConsBySize();		//"<size> - <number of concepts with intents/extents this size>"
	void outputContext();			//create sub-context and output it as cxt file
	void cxtFileOutput();			//output the sorted context file
	void outputJSONConcepts();		//output concepts with many valued original attributes in JSON format
	void outputConceptTreeDendrogram();
	void outputConceptTree();		//output CONCEPT TREE to file in JSON format using object and attribute names:
									//outputs concepts as tree nodes with node number, parent node number, own attributes and own objects
	void outputConceptsNamesCSV();  //outputs concepts as a csv file

	cout << "*** In-Close4 Concept Miner and Concept Tree Builder 64-bit version ***";
	cout << "\n\nCopyright Simon Andrews 2017, Sheffield Hallam University, 2017";
	cout << "\n\nEnter cxt or dat file name including extension: ";
	cin >> fname;

	//find out if file is cxt (FCA) or dat (FIMI) format
	if(fname.substr( fname.length() - 3 ) == "dat") FIMI = true;

	cout << "\nEnter minimum size of intent (no. attributes): ";
	cin  >> minIn;
	cout << "\nEnter minimum size of extent (no. objects): ";
	cin  >> minEx;

	//input context file
	if(FIMI)
		datFileInput();
	else
		cxtFileInput();

	//select format for outputting concepts
	char outcons;
	char outoption = '1';
	cout << "\n\nOutput concepts to file? (y/n): ";
	cin >> outcons;
	if(outcons == 'y'){
		if(!FIMI){
			cout << "\nOutput concepts in which format: \n\t 1) concepts as lists of index numbers of objects and attributes.";
			cout << "\n\t 2) concepts as lists of names of objects and attributes.";
			cout << "\n\t 3) concepts with arrays for many-valued attributes,";
			cout << "\n\t e.g. if the objects share the attributes \"location-Sheffield\" and \"location-Pitsmoor\"";
			cout << "\n\t the corresponding JSON construction will be \"location\" : [\"Sheffield\",\"Pitsmoor\"].";
			cout << "\n\t 4) output CONCEPT TREE to file in JSON format 'A' using object and attribute names.";
			cout << "\n\t\t Visualise output concepts.JSON file at: //homepages.shu.ac.uk/~aceslh/fca/fcaTree.html";
			cout << "\n\t\t Note that tree outputs significantly increase computation time.";
			cout << "\n\t 5) output CONCEPT TREE to file in JSON format 'B' using object and attribute names.";
			cout << "\n\t 6) output concepts as a csv file.";
			cout << "\nEnter 1, 2, 3, 4, 5 or 6: ";
			cin >> outoption;
			while(minIn>0 && (outoption == '4' || outoption == '5')){
				cout << "\n\nTree output is not possible with a minimum size of intent (top of the tree will be missing!)";
				cout << "\n\nPlease select a different format: ";
				cout << "\n\t 1) concepts as lists of index numbers of objects and attributes.";
				cout << "\n\t 2) concepts as lists of names of objects and attributes.";
				cout << "\n\t 3) concepts with arrays for many-valued attributes, e.g. if the objects share the attributes \"location-Sheffield\" and \"location-Pitsmoor\"";
				cout << "\n\t the corresponding JSON construction will be \"location\" : [\"Sheffield\",\"Pitsmoor\"].";
				cout << "\n\t 6) output concepts as a csv file.";
				cout << "\nEnter 1, 2, 3 or 6: ";
				cin >> outoption;
			}
		}
	}

	sSort.startTimer();
	//set initial intent parent to 'no attributes involved'
	for(int i = 0; i < MAX_COLS/64 + 1; i++) Bparent[i] = 0;

	/* Initialse concept 0: the supremum */
	/* A[0] = {0,1,...,m-1} - supremum involves all objects */
	for(int i = 0; i < m; i++) A[i] = i;
	startA[0] = &A[0];
	startA[1] = &A[m];
	/* B[0] = {} - supremum initially has no attributes */
	sizeB[0] = 0;
	startB[0] = &B[0];
	nodeParent[0] = -1; //supremun does not have a parent

	for (int i = 0; i < n; i++) colOriginal[i] = i; //init column index array for sorting

	if((outoption != '4') && (outoption != '5')) {  //normally sort columns in ascedning order for performance
		cout << "\nSorting...";
		sortColumns(); 
	}
	else { //unless a tree is required, in which case sort in descending order to force the most recursion (bushiness)
		cout << "\nSorting...";
		sortColumnsR(); 
	}

	/* write translated context after column sorting, so that 'context' is ready for row sorting*/
	for(int i=0;i<m;i++){
		for(int j=0;j<n;j++){
			if(contextTemp[j][(i>>6)]&(VECTOR_1<<(i%64)))
				context[i][(j>>6)] |= (VECTOR_1<<(j%64));
		}
	}
	DYN_ALIGNED_DELETE_ARR(contextTemp);

	//initialise row pointers and original row indexes
	for(int i=0;i<m;i++) rowOriginal[i]=i;

	if((outoption != '4') && (outoption != '5')) sortRows();

	sSort.stopTimer();

	char sortcxt;
		cout << "\n\nOutput sorted cxt file (y/n): ";
		cin >> sortcxt;
		if(sortcxt == 'y') {
			cout << "\nOutputting sorted cxt...";
			cxtFileOutput();
		}

	cout << "\nMining concepts...";
	sInner.startTimer(); //start inner timing: preprocessing and the mining

	/* we will skip empty columns in processing*/
	startCol = 0;
	while(colSup[startCol] == 0) startCol++;

	/* mine concepts */
	if(outoption == '4')				//dendrogram tree output
		InCloseDendo(0, startCol, Bparent);
	if(minEx && outoption != '4')		//min support and not dendrogram tree output
		InCloseMin(0, startCol, Bparent);
	if(!minEx && outoption != '4')		//no min support and not dendrogram tree output
		InClose(0, startCol, Bparent);

	sInner.stopTimer(); //report inner time

	cout << "\n\nSort time     \t\t : " << sSort.getElapsedTime() << " seconds";
	cout << "\nConcept mining time      : " << sInner.getElapsedTime() << " seconds";
	cout << "\nTotal time \t\t : " << sSort.getElapsedTime()+sInner.getElapsedTime() << " seconds";

	calcAandBsizes();

	outputNoConsBySize();//and count concepts

	cout << "\n\nNumber of concepts: " << numcons;

	//call the desired concept output procedure
	if(outcons == 'y'){
		if(FIMI){
			outputConcepts();
		}
		else{
			switch(outoption){
				case '1' :
					outputConcepts();
					break;
				case '2' :
					outputConceptsNames();
					break;
				case '3' :
					outputJSONConcepts();
					break;
				case '4' :
					outputConceptTreeDendrogram();
					break;
				case '5' :
					outputConceptTree();
					break;
				case '6' :
					outputConceptsNamesCSV();
					break;
				default :
					outputConceptsNames();
				}
		}
		cout << "Done.";
	}

	//if minimum support has been specified, output the reduced context file
	if(minIn>0 || minEx > 0)
		outputContext();

	//cout << "\n\nIntersections =  " << inters;
	//cout << "\n\nCanon Tests =  " << test;

	cout << "\n\nHit <enter> to finish";
	while ( !_kbhit());
	//delete [] context;
	DYN_ALIGNED_DELETE_ARR(context);
	delete onames;
	delete anames;
}

//********** END MAIN *********************************************************

void InClose(const int c, const int y, VECTOR_TYPE *Bparent)
/* c: concept number, y: attribute number, Bparent: parent intent in Boolean form */
{
	bool IsCannonical(const int y,  const int * endAhighc, const VECTOR_TYPE Bparent[]);
	/* y: attribute number, endAhighc: pointer to end of the next extent to be created */
	/* Bchild: the current intent in Boolean form (to skip columns when checking cannonocity of any 'new' extent) */

	int Bchildren[MAX_COLS];							//the attributes that will spawn new concepts
	int numchildren = 0;								//the number of new concepts spawned from current one
	int Cnums[MAX_COLS];								//the concept no.s of the spawned concepts
	VECTOR_TYPE Bchild[MAX_COLS/64 + 1];				//the current intent in Boolean form

	
	/*********************** MAIN LOOP *********************************************************
		interate across attribute columns forming column intersetcions with current extent
	********************************************************************************************/
	int sizeAc = startA[c+1]-startA[c];			//calculate the size of current extent
	//for(int j = y; j >= 0; --j)	{
	for(int j = y; j < n; j++) {
		if(!(Bparent[j>>6] & (VECTOR_1 << (j % 64)))){
			//inters++;
			//if attribute is not an inherited one
			int * Ac = startA[c];						//pointer to start of current extent
			int * aptr = startA[highc];					//pointer to start of next extent to be created

			/* iterate across objects in current extent to find them in current attribute column */
			for(int i = sizeAc; i > 0; i--){
				if(context[*Ac][j>>6] & (VECTOR_1 << (j % 64))){//context[*Ac][J] where J is byte J div 8, bit J mod 8
					*aptr = *Ac;						//add object to new extent (intersection)
					aptr++;
				}
				Ac++;									//next object
			}

			int size = aptr - startA[highc];			//calculate size of intersection

			if(size==0){
				Bparent[j>>6] = Bparent[j>>6] | (VECTOR_1 << (j % 64));	//intersection is empty, so the column can be ignored in subsequent levels
			}
			else {
				if(size < sizeAc){
					//test++;
					if(IsCannonical(j,aptr,Bparent)){	//if the intersection is a new extent, note the child for later spawning:
						Bchildren[numchildren] = j;			//note where (attribute column) it was found,
						Cnums[numchildren++] = highc;		//note the concept number,
						nodeParent[highc] = c;				//note the parent concept number and
						startA[++highc] = aptr;				//note the start of the new extent in A.
						/*if(highc == MAX_CONS){
							cout << "ooops";
						}*/
					}
				}
				else {		//size == sizeAc: extent is unchanged
					*bptr = j;							//add current attribute to intent
					bptr++;
					Bparent[j>>6] = Bparent[j>>6] | (VECTOR_1 << (j % 64));		//record that the attribute will be inherited by any child concepts
					sizeBnode[c]++;						//increment the number of attributes at this node in the B tree
				}
			}
		}
	}
	/* spawn child concepts from this parent */
	for(int i = numchildren-1; i >= 0 ; i--){
		memcpy(Bchild,Bparent,nArray*8);	//set the child attributes to the parent ones (inheritance)
		Bchild[Bchildren[i]>>6] = Bchild[Bchildren[i]>>6] | (VECTOR_1 << (Bchildren[i] % 64));
		*bptr = Bchildren[i];
		startB[Cnums[i]] = bptr;	//set the start of the intent in B tree	
		bptr++;
		sizeBnode[Cnums[i]]++;			
		InClose(Cnums[i], Bchildren[i]+1, Bchild);		//close the child concept
	}													
}


/* InCloseMin() is identical to InClose() apart from test for min support instead of test for not-empty intersection */
/* which gives a small performance benefit */
void InCloseMin(const int c, const int y, VECTOR_TYPE *Bparent)
/* c: concept number, y: attribute number, Bparent: parent intent in Boolean form */
{
	bool IsCannonical(const int y,  const int * endAhighc, const VECTOR_TYPE Bparent[]);
	/* y: attribute number, endAhighc: pointer to end of the next extent to be created */
	/* Bchild: the current intent in Boolean form (to skip columns when checking cannonocity of any 'new' extent) */

	int Bchildren[MAX_COLS];							//the attributes that will spawn new concepts
	int numchildren = 0;								//the number of new concepts spawned from current one
	int Cnums[MAX_COLS];								//the concept no.s of the spawned concepts
	VECTOR_TYPE Bchild[MAX_COLS/64 + 1];				//the current intent in Boolean form

	
	/*********************** MAIN LOOP *********************************************************
		interate across attribute columns forming column intersetcions with current extent
	********************************************************************************************/
	int sizeAc = startA[c+1]-startA[c];			//calculate the size of current extent
	//for(int j = y; j >= 0; --j)	{
	for(int j = y; j < n; j++) {
		if(!(Bparent[j>>6] & (VECTOR_1 << (j % 64)))){
			//inters++;
			//if attribute is not an inherited one
			int * Ac = startA[c];						//pointer to start of current extent
			int * aptr = startA[highc];					//pointer to start of next extent to be created

			/* iterate across objects in current extent to find them in current attribute column */
			for(int i = sizeAc; i > 0; i--){
				if(context[*Ac][j>>6] & (VECTOR_1 << (j % 64))){//context[*Ac][J] where J is byte J div 8, bit J mod 8
					*aptr = *Ac;						//add object to new extent (intersection)
					aptr++;
				}
				Ac++;									//next object
			}

			int size = aptr - startA[highc];			//calculate size of intersection

			if(size < minEx){
				Bparent[j>>6] = Bparent[j>>6] | (VECTOR_1 << (j % 64));	//intersection is smaller than min support, so the column can be ignored in subsequent levels
			}
			else {
				if(size < sizeAc){
					//test++;
					if(IsCannonical(j,aptr,Bparent)){	//if the intersection is a new extent, note the child for later spawning:
						Bchildren[numchildren] = j;			//note where (attribute column) it was found,
						Cnums[numchildren++] = highc;		//note the concept number,
						nodeParent[highc] = c;				//note the parent concept number and
						startA[++highc] = aptr;				//note the start of the new extent in A.
						/*if(highc == MAX_CONS){
							cout << "ooops";
						}*/
					}
				}
				else {		//size == sizeAc: extent is unchanged
					*bptr = j;							//add current attribute to intent
					bptr++;
					Bparent[j>>6] = Bparent[j>>6] | (VECTOR_1 << (j % 64));		//record that the attribute will be inherited by any child concepts
					sizeBnode[c]++;						//increment the number of attributes at this node in the B tree
				}
			}
		}
	}
	/* spawn child concepts from this parent */
	for(int i = numchildren-1; i >= 0 ; i--){
		memcpy(Bchild,Bparent,nArray*8);	//set the child attributes to the parent ones (inheritance)
		Bchild[Bchildren[i]>>6] = Bchild[Bchildren[i]>>6] | (VECTOR_1 << (Bchildren[i] % 64));
		*bptr = Bchildren[i];
		startB[Cnums[i]] = bptr;	//set the start of the intent in B tree	
		bptr++;
		sizeBnode[Cnums[i]]++;			
		InClose(Cnums[i], Bchildren[i]+1, Bchild);		//close the child concept
	}													
}




void InCloseDendo(const int c, const int y, const VECTOR_TYPE *Bparent)
/* c: concept number, y: attribute number, Bparent: parent intent in Boolean form.							 */
/* This version of In-Close routine collects the child concept data required for later JSON dendogram output */
{
	bool IsCannonical(const int y,  const int * endAhighc, const VECTOR_TYPE Bchild[]);
	/* y: attribute number, endAhighc: pointer to end of the next extent to be created */
	/* Bchild: the current intent in Boolean form (to skip columns when checking cannonocity of any 'new' extent) */

	int Bchildren[MAX_COLS];							//the attributes that will spawn new concepts
	int numchildren = 0;								//the number of new concepts spawned from current one
	int Cnums[MAX_COLS];								//the concept no.s of the spawned concepts
	VECTOR_TYPE Bchild[MAX_COLS/64 + 1];				//the current intent in Boolean form

	memcpy(Bchild,Bparent,nArray*8);	//set the child attributes to the parent ones (inheritance)

	if(c){		//if not concept 0, add the spawning attribute to intent
		Bchild[(y-1)>>6] = Bchild[(y-1)>>6] | (VECTOR_1 << ((y-1) % 64));
		*bptr = y-1;
		bptr++;
		sizeBnode[c]++;
	}
	/*********************** MAIN LOOP *********************************************************
		interate across attribute columns forming column intersetcions with current extent
	********************************************************************************************/
	int sizeAc = startA[c+1]-startA[c];			//calculate the size of current extent
	//for(int j = y; j >= 0; --j)	{
	for(int j = y; j < n; j++) {
		if(!(Bchild[j>>6] & (VECTOR_1 << (j % 64)))){
			//if attribute is not an inherited one
			int * Ac = startA[c];						//pointer to start of current extent
			int * aptr = startA[highc];					//pointer to start of next extent to be created

			/* iterate across objects in current extent to find them in current attribute column */
			for(int i = sizeAc; i > 0; i--){
				if(context[*Ac][j>>6] & (VECTOR_1 << (j % 64))){//context[*Ac][J] where J is byte J div 8, bit J mod 8
					*aptr = *Ac;						//add object to new extent (intersection)
					aptr++;
				}
				Ac++;									//next object
			}

			int size = aptr - startA[highc];			//calculate size of intersection

			if(size < minEx){
				Bchild[j>>6] = Bchild[j>>6] | (VECTOR_1 << (j % 64));	//intersection is smaller than min supp, so the column can be ignored in subsequent levels
			}
			else {
				if(size < sizeAc){
					if(IsCannonical(j,aptr,Bchild)){	//if the intersection is a new extent, note the child for later spawning:
						Bchildren[numchildren] = j;			//note where (attribute column) it was found,
						Cnums[numchildren++] = highc;		//note the concept number,
						nodeParent[highc] = c;				//note the parent concept number and
						startA[++highc] = aptr;				//note the start of the new extent in A.
					}
				}
				else {		//size == sizeAc: extent is unchanged
					*bptr = j;							//add current attribute to intent
					bptr++;
					Bchild[j>>6] = Bchild[j>>6] | (VECTOR_1 << (j % 64));		//record that the attribute will be inherited by any child concepts
					sizeBnode[c]++;						//increment the number of attributes at this node in the B tree
				}
			}
		}
	}
	/* record child concepts for later deondoram tree output */
	numchildrenof[c] = numchildren;
	startofchildren[c] = cptr;
	for(int i = numchildren-1; i >= 0 ; i--){
		*cptr = Cnums[i];
		cptr++;
	}

	/* spawn child concepts from this parent */
	for(int i = numchildren-1; i >= 0 ; i--){
		startB[Cnums[i]] = bptr;						//set the start of the intent in B tree
		InCloseDendo(Cnums[i], Bchildren[i]+1, Bchild);		//close the child concept (next closure starts at j-1 to
	}													//avoid having to create this intersection again)
}


bool IsCannonical(const int y, const int * endAhighc, const VECTOR_TYPE Bchild[])
/* y: attribute number, endAhighc: pointer to end of the next extent to be created */
/* Bchild: the current intent in Boolean form (to skip columns when checking cannonocity of any 'new' concept) */
{

	//initialse Bmask
	VECTOR_TYPE Bmask[MAX_COLS/64 + 1];
	int p;
	for(p = 0; p < y>>6; p++){
		Bmask[p]=~Bchild[p]; //invert 64 bit chunks of current intent
	}
	Bmask[p]= ~Bchild[p] & ((VECTOR_1 << (y % 64))-1); //invert last 64 bits up to current attribute

	for(p=0; p <= y>>6; p++){
		int i;
		int * Ahighc = startA[highc];	//find start of extent
		for(i = endAhighc - Ahighc; i > 0; i--){	 //iterate from number of objects downto zero
			Bmask[p] = Bmask[p] & context[*Ahighc][p]; //apply mask to context (testing 64 cells at a time)
			if(!Bmask[p])break;		//if there is nothing still true then stop looking down this 64 columns
			Ahighc++;				//otherwise, next object
		}
		if(i==0) return(false);
	}
	return(true);	//if intersection is not found, it is cannonical
}


void outputConcepts() //output concepts to file in JSON format
{
	string strData = "";
	char ibuffer[40];
	cout << "\n\nOutputting concepts to file...";
	FILE *fp1;
	fopen_s(&fp1, "concepts.json", "w");

	fputs("{\n\t\"Concepts\":\n\t[\n",fp1);

	int conId = 0;

	/* for each concept */
	for(int c = 0; c < highc; c++)
	{
		if(sizeB[c] >= minIn && sizeA[c] >= minEx)
		{
			if(conId == 0)
				strData+= "\t\t{\n";
			else
				strData+= ",\n\n\t\t{\n";

			conId++;
			strData+= "\t\t\t\"ConceptId\" : ";
			_itoa_s(conId,ibuffer,40,10);
			strData += ibuffer;
			strData += ",\n";

			strData += "\t\t\t\"attributes\" : [";

			/* traverse B tree to obtain intent */
			int i = c;									//set i to concept
			bool firstAtt = true;
			while(i >= 0){								//when i==-1, head node is reached
				short int *bptr = startB[i];			//point to start of attributes in first node of this intent
				for(int j = 0; j < sizeBnode[i]; j++){	//iterate attributes at this node
					if(!firstAtt) strData+=",";
					firstAtt = false;
					_itoa_s(colOriginal[*bptr],ibuffer,40,10);//convert attribute no. to ASCII
					strData+= ibuffer;
					bptr++;
				}
				i = nodeParent[i];						//point to next node
			}
			strData+= "],\n";							//end of attributes

			strData+= "\t\t\t\"objects\" : [";
			/* obtain extent from A */
			int * aptr = startA[c];						//point to start of extent
			for(int j = 0; j < sizeA[c]; j++){			//iterate objects
				if(j>0) strData+=",";
				_itoa_s(rowOriginal[*aptr],ibuffer,40,10);	//convert object no. to ASCII
				strData+=ibuffer;
				aptr++;
			}
			strData+= "]\n\t\t}";
		}
		fputs(strData.c_str(),fp1);
		strData="";
	}
	fputs("\n\t]\n}",fp1);
	fclose(fp1);
}

void outputConceptsNames() //output concepts to file in JSON format using object and attribute names
{
	string strData = "";
	char ibuffer[40];
	cout << "\n\nOutputting concepts to file...";
	FILE *fp1;
	fopen_s(&fp1, "concepts.json", "w");

	fputs("{\n\t\"Concepts\":\n\t[\n",fp1);

	//test to see if (all) objects are numbers
	string str;
	bool obs_are_numbers = true;
	for(int i=0; i<m;i++){
		str = onames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) obs_are_numbers = false;
	}

	//test to see if (all) attributes are numbers
	bool atts_are_numbers = true;
	for(int i=0; i<n;i++){
		str = anames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) atts_are_numbers = false;
	}

	int conId = 0;

	/* for each concept */
	for(int c = 0; c < highc; c++)
	{
		if(sizeB[c] >= minIn && sizeA[c] >= minEx)
		{
			if(conId == 0)
				strData+= "\t\t{\n";
			else
				strData+= ",\n\n\t\t{\n";

			conId++;
			strData+= "\t\t\t\"ConceptId\" : ";
			_itoa_s(conId,ibuffer,40,10);
			strData += ibuffer;
			strData += ",\n";

			strData += "\t\t\t\"attributes\" : [";

			/* traverse B tree to obtain intent */
			int i = c;									//set i to concept
			bool firstAtt = true;
			while(i >= 0){								//when i==-1, head node is reached
				short int *bptr = startB[i];			//point to start of attributes in first node of this intent
				for(int j = 0; j < sizeBnode[i]; j++){	//iterate attributes at this node
					if(!firstAtt) strData+=",";
					firstAtt = false;

					if(atts_are_numbers){
						strData+=anames[colOriginal[* bptr]];
					}
					else{
						strData += "\"";
						strData+=anames[colOriginal[* bptr]];
						strData += "\"";
					}
					bptr++;
				}
				i = nodeParent[i];						//point to next node
			}
			strData+= "],\n";							//end of attributes

			strData+= "\t\t\t\"objects\" : [";
			/* obtain extent from A */
			int * aptr = startA[c];						//point to start of extent
			for(int j = 0; j < sizeA[c]; j++){			//iterate objects
				if(j>0) strData+=",";

				if(obs_are_numbers){
					strData+=onames[rowOriginal[*aptr]];
				}
				else{
						strData += "\"";
						strData+=onames[rowOriginal[*aptr]];
						strData += "\"";
				}
				aptr++;
			}
			strData+= "]\n\t\t}";
		}
		fputs(strData.c_str(),fp1);
		strData="";
	}
	fputs("\n\t]\n}",fp1);
	fclose(fp1);
}

void outputConceptsNamesCSV() //output concepts to file in JSON format using object and attribute names
{
	string strData = "";
	char ibuffer[40];
	cout << "\n\nOutputting concepts to file...";
	FILE *fp1;
	fopen_s(&fp1, "concepts.csv", "w");

	//test to see if (all) objects are numbers
	string str;
	bool obs_are_numbers = true;
	for(int i=0; i<m;i++){
		str = onames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) obs_are_numbers = false;
	}

	//test to see if (all) attributes are numbers
	bool atts_are_numbers = true;
	for(int i=0; i<n;i++){
		str = anames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) atts_are_numbers = false;
	}

	int conId = 0;

	/* for each concept */
	for(int c = 0; c < highc; c++)
	{
		if(sizeB[c] >= minIn && sizeA[c] >= minEx)
		{
			
			/* traverse B tree to obtain intent */
			int i = c;									//set i to concept
			bool firstAtt = true;
			while(i >= 0){								//when i==-1, head node is reached
				short int *bptr = startB[i];			//point to start of attributes in first node of this intent
				for(int j = 0; j < sizeBnode[i]; j++){	//iterate attributes at this node
					if(!firstAtt) strData+=",";
					firstAtt = false;

					if(atts_are_numbers){
						strData+=anames[colOriginal[* bptr]];
					}
					else{
						//strData += "\'";
						strData+=anames[colOriginal[* bptr]];
						//strData += "\'";
					}
					bptr++;
				}
				i = nodeParent[i];						//point to next node
			}
			strData+= "\n";

			/* obtain extent from A */
			int * aptr = startA[c];						//point to start of extent
			for(int j = 0; j < sizeA[c]; j++){			//iterate objects
				if(j>0) strData+=",";
				if(obs_are_numbers){
					strData+=onames[rowOriginal[*aptr]];
				}
				else{
						//strData += "\'";
						strData+=onames[rowOriginal[*aptr]];
						//strData += "\'";
				}
				aptr++;
			}
			strData+= "\n";

		}
		fputs(strData.c_str(),fp1);
		strData="";
	}
	fclose(fp1);
}

void outputConceptTreeDendrogram()	//output CONCEPT TREE to file in JSON format using object and attribute names
									// outputs concepts with inline children - D3 dendogram format
{
	void outputConceptTreeFrom(int con, bool oan, bool aan, FILE* fp1, string tabs);

	cout << "\n\nOutputting concepts to file...";
	FILE *fp1;
	fopen_s(&fp1, "concepts.json", "w");
	string tabs = "";

	//test to see if (all) objects are numbers
	string str;
	bool obs_are_numbers = true;
	for(int i=0; i<m;i++){
		str = onames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) obs_are_numbers = false;
	}

	//test to see if (all) attributes are numbers
	bool atts_are_numbers = true;
	for(int i=0; i<n;i++){
		str = anames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) atts_are_numbers = false;
	}

	/* intialise size extent of own objects array */
	for(int c = 0; c < highc; c++)
		sizeOwnA[c] = sizeA[c];

	/* traverse B tree to remove inhertited objects from nodes own objects */
	for(int child = 0; child < highc; child++) {	//for each concept (node)
		int parent = nodeParent[child];
		if(parent >= 0 && sizeA[child] >= minEx){	//if child has a parent and satisfies min support
			int * aptr = startA[child];				//point to start of extent
			for(int i = 0; i < sizeA[child]; i++) { // for each object in child (or can we use sizeOwnA ?)
				int * oaptr = startA[parent];		//point to start of own extent of parent
				bool found = false;
				for(int q = 0; q < sizeOwnA[parent]; q++) { //search through own extent of parent to find child object
					if(*aptr == *oaptr) {
						found = true;
						break;
					}
					else {
						oaptr++;
					}

				}
				if(found){ //swap the found object with the last object in parent own extent
					int *endOwnAptr = startA[parent] + sizeOwnA[parent] - 1;
					int swap = *oaptr;
					*oaptr = *endOwnAptr;
					*endOwnAptr = swap;
					sizeOwnA[parent]--; //and decrement size of parent own extent
				}
				aptr++; //next object in child
			}
		}
	}

	bool firstNode = true;

	outputConceptTreeFrom(0, obs_are_numbers, atts_are_numbers, fp1, tabs);

	fputs("\n}",fp1);
	fclose(fp1);

}

void outputConceptTreeFrom(int c, bool obs_are_numbers, bool atts_are_numbers, FILE* fp1, string tabs)
{
	
	string strData = "";
	char ibuffer[40];
	tabs+="\t";

	if(sizeB[c] >= minIn && sizeA[c] >= minEx)
	{
		strData+= "{\n";
		strData+= tabs;
		strData+= "\"Node\" : ";
		_itoa_s(c,ibuffer,40,10);
		strData += ibuffer;
		strData += ",\n";

		strData+= tabs;
		strData += "\"attributes\" : [";

		/* obtain own attributes */
		int i = c;									//set i to concept
		bool firstAtt = true;
		//while(i >= 0){							//when i==-1, head node is reached
			short int *bptr = startB[i];			//point to start of attributes in first node of this intent
			for(int j = 0; j < sizeBnode[i]; j++){	//iterate attributes at this node
				if(!firstAtt) strData+=",";
				firstAtt = false;

				if(atts_are_numbers){
					strData+=anames[colOriginal[* bptr]];
				}
				else{
					strData += "\"";
					strData+=anames[colOriginal[* bptr]];
					strData += "\"";
				}
				bptr++;
			}
			//i = nodeParent[i];						//point to next node
		//}
		strData+= "],\n";							//end of attributes

		//output JSON array containing the objects in the extent
		strData+= tabs;
		strData += "\"objects\" : [";
		int * aptr = startA[c];						//point to start of extent
		for(int j = 0; j < sizeA[c]; j++){			//iterate objects
			if(j>0) strData+=",";
			if(obs_are_numbers){
				strData+=onames[rowOriginal[*aptr]];
			}
			else{
					strData += "\"";
					strData+=onames[rowOriginal[*aptr]];
					strData += "\"";
			}
			aptr++;
		}
		strData += "],\n";

		strData+= tabs;
		strData+= "\"own_objects\" : [";
		/* obtain own objects from A */
		aptr = startA[c];						//point to start of extent
		for(int j = 0; j < sizeOwnA[c]; j++){	//iterate objects of own extent
			if(j>0) strData+=",";
			if(obs_are_numbers){
				strData+=onames[rowOriginal[*aptr]];
			}
			else{
					strData += "\"";
					strData+=onames[rowOriginal[*aptr]];
					strData += "\"";
			}
			aptr++;
		}
		strData+= "],\n";
		strData+= tabs;
		strData+= "\"ObjectCount\" : \"";
		_itoa_s(sizeA[c],ibuffer,40,10);
		strData+= ibuffer;
		strData+= " | ";
		_itoa_s(percentA[c],ibuffer,40,10);
		strData+= ibuffer;
		strData+= "%\"";

		if(numchildrenof[c] > 0){
			strData+= ",\n\n";
			strData+= tabs;
			strData+="\"children\": [";
		}

		fputs(strData.c_str(),fp1);

		/* Output children */
		int * ch = startofchildren[c];
		for(int i = 0; i < numchildrenof[c]; i++){
			outputConceptTreeFrom(*ch, obs_are_numbers, atts_are_numbers, fp1, tabs);
			ch++;
			fputs("\n",fp1);
			fputs(tabs.c_str(),fp1);
			if(i==numchildrenof[c]-1)
				fputs("}]",fp1);
			else 
				fputs("},",fp1);
		}
	}
}

void outputConceptTree()	//output CONCEPT TREE to file in JSON format using object and attribute names
							//outputs concepts as tree nodes with node number, parent node number, own attributes and own objects
{
	string strData = "";
	char ibuffer[40];
	cout << "\n\nOutputting concepts to file...";
	FILE *fp1;
	fopen_s(&fp1, "concepts.json", "w");

	fputs("{\n\t\"Concepts\":\n\t[\n",fp1);

	//test to see if (all) objects are numbers
	string str;
	bool obs_are_numbers = true;
	for(int i=0; i<m;i++){
		str = onames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) obs_are_numbers = false;
	}

	//test to see if (all) attributes are numbers
	bool atts_are_numbers = true;
	for(int i=0; i<n;i++){
		str = anames[i];
		if(str.find_first_not_of("0123456789") != std::string::npos) atts_are_numbers = false;
	}


	/* intialise size extent of own objects array */
	for(int c = 0; c < highc; c++)
		sizeOwnA[c] = sizeA[c];

	/* traverse B tree to remove inhertited objects from nodes own objects */
	for(int child = 0; child < highc; child++) {	//for each concept (node)
		int parent = nodeParent[child];
		if(parent >= 0 && sizeA[child] >= minEx){	//if child has a parent and satisfies min support
			int * aptr = startA[child];				//point to start of extent
			for(int i = 0; i < sizeA[child]; i++) { // for each object in child (or can we use sizeOwnA ?)
				int * oaptr = startA[parent];		//point to start of own extent of parent
				bool found = false;
				for(int q = 0; q < sizeOwnA[parent]; q++) { //search through own extent of parent to find child object
					if(*aptr == *oaptr) {
						found = true;
						break;
					}
					else {
						oaptr++;
					}

				}
				if(found){ //swap the found object with the last object in parent own extent
					int *endOwnAptr = startA[parent] + sizeOwnA[parent] - 1;
					int swap = *oaptr;
					*oaptr = *endOwnAptr;
					*endOwnAptr = swap;
					sizeOwnA[parent]--; //and decrement size of parent own extent
				}
				aptr++; //next object in child
			}
		}
	}

	bool firstNode = true;

	/* for each concept */
	for(int c = 0; c < highc; c++)
	{
		if(sizeB[c] >= minIn && sizeA[c] >= minEx)
		{
			if(firstNode){
				firstNode = false;
				strData+= "\t\t{\n";
			}
			else
				strData+= ",\n\n\t\t{\n";

			strData+= "\t\t\t\"Node\" : ";
			_itoa_s(c,ibuffer,40,10);
			strData += ibuffer;
			strData += ",\n";

			strData+= "\t\t\t\"ParentNode\" : ";
			_itoa_s(nodeParent[c],ibuffer,40,10);
			strData += ibuffer;
			strData += ",\n";

			strData += "\t\t\t\"attributes\" : [";

			/* traverse B tree to obtain intent */
			int i = c;									//set i to concept
			bool firstAtt = true;
			//while(i >= 0){							//when i==-1, head node is reached
				short int *bptr = startB[i];			//point to start of attributes in first node of this intent
				for(int j = 0; j < sizeBnode[i]; j++){	//iterate attributes at this node
					if(!firstAtt) strData+=",";
					firstAtt = false;

					if(atts_are_numbers){
						strData+=anames[colOriginal[* bptr]];
					}
					else{
						strData += "\"";
						strData+=anames[colOriginal[* bptr]];
						strData += "\"";
					}
					bptr++;
				}
				//i = nodeParent[i];						//point to next node
			//}
			strData+= "],\n";							//end of attributes

			//output JSON array containing the objects in the extent
				strData += "\t\t\t\"objects\" : [";
				int * aptr = startA[c];						//point to start of extent
				for(int j = 0; j < sizeA[c]; j++){			//iterate objects
					if(j>0) strData+=",";
					if(obs_are_numbers){
						strData+=onames[rowOriginal[*aptr]];
					}
					else{
						strData += "\"";
						strData+=onames[rowOriginal[*aptr]];
						strData += "\"";
					}
					aptr++;
				}
				strData += "],\n\t\t";

			strData+= "\t\"own_objects\" : [";
			/* obtain own objects from A */
			aptr = startA[c];						//point to start of extent
			for(int j = 0; j < sizeOwnA[c]; j++){	//iterate objects of own extent
				if(j>0) strData+=",";

				if(obs_are_numbers){
					strData+=onames[rowOriginal[*aptr]];
				}
				else{
						strData += "\"";
						strData+=onames[rowOriginal[*aptr]];
						strData += "\"";
				}
				aptr++;
			}
			strData+= "],\n\t\t\t";
			strData+= "\"ObjectCount\" : \"";
			_itoa_s(sizeA[c],ibuffer,40,10);
			strData+= ibuffer;
			strData+= " | ";
			_itoa_s(percentA[c],ibuffer,40,10);
			strData+= ibuffer;
			strData+= "%\"\n\t\t}";
		}
		fputs(strData.c_str(),fp1);
		strData="";
	} //END for each concept

	fputs("\n\t]\n}",fp1);
	fclose(fp1);
}

void outputJSONConcepts() //output concepts to file using object and attribute names in JSON format
{
	string strData; //string for file ouput
	//create arrays for JSON fields and (many) values
	/*string Jfields[3000];
	string Jvalues[3000][10];*/

	int numJfields = 1;			//the number of JSON fields will be equal to the number of orignal many-valued attributes
	int numvals[1000];			//the number of values for each field (for many-valued attributes)
	int numJSONconcepts = 0;

	/*Initialise Context Output */
	/* flags to use to control subsequent file output: only output rows and columns that are not empty */
	bool colHasSupport[MAX_COLS];
	bool* rowHasSupport = new bool[MAX_ROWS];
	for(int i=0;i<m;i++) rowHasSupport[i]=false;
	for(int j=0;j<n;j++) colHasSupport[j]=false;
	/* zeroise context */
	for (int i = 0;i < m;i++)
		for(int j = 0;j < nArray; j++)
			context[i][j]=0;


	FILE *fp1;
	fopen_s(&fp1, "concepts.json", "w");
	fputs("{\n\t\"Concepts\":\n\t[",fp1);

	cout << "\n\nIf attributes have been formed from many-valued attributes, enter delimiter character, else hit enter: ";
	bool manyvalued;
	getchar();
	char delim = getchar();
	if(delim=='\n')
		manyvalued = false;
	else
		manyvalued = true;

	//find all the many-valued attribute names e.g. get gender from gender-male and gender-female
	string JFV = anames[0];
	int pos = JFV.find(delim);
	Jfields[0] = JFV.substr(0,pos);
	for(int i = 1; i<n; i++){
		string JFV = anames[i];
		int pos = JFV.find(delim);
		string JF = JFV.substr(0,pos);
		int found = 0;
		for(int k=0; k<numJfields; k++){
			if(!JF.compare(Jfields[k])){
				found = 1;
				break;
			}
		}
		if(!found){
			Jfields[numJfields] = JF;
			numJfields++;
		}
	}

	cout << "\n\nOutputting Concepts to file in JSON format...";

	bool firstConcept = true;
	/* for each concept */
	for(int c = 0; c < highc; c++){
		strData = "";
		if(sizeB[c] >= minIn && sizeA[c] >= minEx){
			for(int i=0; i <1000; i++) numvals[i]=0;			//initialise the number of values for each attribute to 0
			for(int i=0; i<numJfields; i++) Jvalues[i][0]="";	//initailse JSON field values to empty strings
			/* traverse B tree to obtain intent */
			int i = c;											//set i to concept
			while(i >= 0){										//when i==-1, head node is reached
				short int * bptr = startB[i];					//point to start of attributes in first node of this intent
				for(int j = 0; j < sizeBnode[i]; j++){			//iterate attributes at this node
					string JFV = anames[colOriginal[* bptr]];	//JFV is the JSON field and value, eg gender-male
					int pos = JFV.find(delim);
					string JF = JFV.substr(0,pos);				//get JF, the JSON field, eg gender
					string JV = JFV.substr(pos+1,string::npos);	//get JV, the JSON value, eg male
					for(int k=0; k<numJfields; k++){            //find the field in the list of fields
						if(!JF.compare(Jfields[k])) {
							if(manyvalued)						//if data is not many valued and does not have delimeter, the JF is the whole of the attribute name
								Jvalues[k][numvals[k]++] = JV;	//so add the attribute value to the list of values for field k
							else{
								Jvalues[k][0] = "true";			//otherwise (for Boolean attributes) the value is just 'true'
								numvals[k]=1;
							}
						}
					}
					bptr++;
				}
				i = nodeParent[i];								//point to next node
			}

				/* For Threat Context Output, traverse B tree to obtain attributes */
				int k = c;											//set i to concept
				while(k >= 0){										//when i==-1, head node is reached
					short int *bptr = startB[k];					//point to start of attributes in first node of this intent
					for(int j = 0; j < sizeBnode[k]; j++){			//iterate attributes at this node
						int b = *bptr;								//obtain original attribute index
						colHasSupport[b]=true;
						bptr++;
						/* obtain extent from A */
						int * aptr = startA[c];						//point to start of extent
						for(int i = 0; i < sizeA[c]; i++){			//iterate objects
							context[*aptr][b>>6] |= (VECTOR_1 << (b%64));	//set context cell to true for this (ob,at) pair
							rowHasSupport[*aptr]=true;
							aptr++;
						}
					}
					k = nodeParent[k];								//point to next node
				}

				/* For Concept Output: */
				numJSONconcepts++;
				//output Concept ID

				char cnum[10];
				if(firstConcept)
					strData = "\n\t\t{\n";
				else
					strData = ",\n\n\t\t{\n";

				strData += "\t\t\t\"ConceptId\" : ";
				_itoa_s(c,cnum,10,10);
				strData += cnum;
				strData += ",";

				//output concept intent (attributes as JSON field:value pairs)
				strData += "\n\t\t\t\"attributes\" : {";
				for(int k=0; k<numJfields; k++){
					strData += "\n\t\t\t\t\"";
					strData += Jfields[k];
					strData += "\"";
					strData += " : ";
					strData += "[";
					for(int i=0; i<numvals[k]; i++){
						if(i>0) strData += ",";
						strData += "\"";
						strData += Jvalues[k][i];
						strData += "\"";
					}
					strData += "]";
					if(k<numJfields-1) strData += ",";
				}
				strData += "\n\t\t\t\t},";

				//ouput frequency (number of objects)
				strData += "\n\t\t\t\"frequency\" : ";
				_itoa_s(sizeA[c],cnum,10,10);
				strData += cnum;
				strData += ",";

				//output JSON array containing the objects
				strData += "\n\t\t\t\"objects\" : [";
				int * aptr = startA[c];						//point to start of extent
				for(int j = 0; j < sizeA[c]; j++){			//iterate objects
					if(j>0) strData+=",";
					strData += "\"";
					strData+=onames[rowOriginal[*aptr]];
					strData += "\"";
					aptr++;
				}
				strData += "]\n\t\t}";
				fputs(strData.c_str(),fp1);
				firstConcept = false;
			}
		}

	fputs("\n\t]\n}",fp1);
	fclose (fp1);

	/*For Context Ouput, count new number of objects and attibutes in reduced context*/
	int newm=0;
	int newn=0;
	for(int i=0;i<m;i++) if(rowHasSupport[i]) newm++;
	for(int j=0;j<n;j++) if(colHasSupport[j]) newn++;

	/* output reduced-context Burmeister file */
	FILE *fpsc;
	fopen_s(&fpsc, "context.cxt", "w");

	/* output header: B numobs numats */
	fputs("B\n\n",fpsc);
	char nobs[10], nats[10];
	_itoa_s(newm,nobs,10);
	_itoa_s(newn,nats,10);
	fputs(nobs,fpsc);
	fputs("\n",fpsc);
	fputs(nats,fpsc);
	fputs("\n\n",fpsc);

	/* output names of objects and attributes */
	for(int i = 0; i < m; i++){
		if(rowHasSupport[i]){
			fputs(onames[rowOriginal[i]],fpsc);
			fputs("\n",fpsc);
		}
	}
	for(int j = 0; j < n; j++){
		if(colHasSupport[j]){
			fputs(anames[colOriginal[j]],fpsc);
			fputs("\n",fpsc);
		}
	}

	/* output reduced context grid - only output non-empty rows/cols	*/
	/* note that the (new) context will be column-sorted in dec order	*/
	char *instance;
	int instanceSize = (newn+2);
	instance = new char[instanceSize];
	ZeroMemory(instance,sizeof(char)*instanceSize);
	for(int i = 0; i < m; i++){
		int numchars=0;
		if(rowHasSupport[i]){
			for(int j = 0; j < n; j++){
				if(colHasSupport[j]){
					if(context[i][j>>6]&(VECTOR_1<<(j%64)))
						instance[numchars++] = 'X';
					else
						instance[numchars++] = '.';
				}
			}
			instance[newn] = '\n';
			fputs(instance,fpsc);
		}
	}
	fclose(fpsc);
}


void datFileInput() {

	int i,j; // object and attribute counters
	ifstream datFile;
	datFile.open(fname.c_str());

	cout << "\nReading data...";

	//get number of objects (= number of lines in file)
	m=std::count(istreambuf_iterator<char>(datFile), istreambuf_iterator<char>(), '\n');

	//cout << "\nnumber of objects is " << m;

	// get all and number of attributes
	vector<string> attributes;

	datFile.clear();
	datFile.seekg (0, ios::beg);

	string line;
	while(getline(datFile,line)) {

		istringstream s(line);
		string cell;

		while(getline(s, cell, ' ')) {
			vector<string>::iterator i = find(attributes.begin(), attributes.end(), cell);
			if(i == attributes.end())
				attributes.push_back(cell);
		}

	}

	n = attributes.size();

	//cout << "\nnumber of attributes is " << n;

	/* create temporary context for sorting*/
	mArray = (m-1)/64 + 1;							//calculate size of second dimension (objects) - 1bit per object
	contextTemp = DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(n);				//create one dimension of the temporary context
	for (j = 0;j < n;j++){							//for each attribute
		contextTemp[j] = DYN_ALIGNED_VECTOR_TYPE_ARR(mArray);	//create a row of objects
		for(i=0;i<mArray;i++) contextTemp[j][i]=0;
	}

	/* create context */
	nArray = (n-1)/64 + 1;						//calculate size of second dimension (attributes) - 1bit per object
	context = DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(m);				//create one dimension of the context
	for (i = 0;i < m;i++){						//for each object
		context[i] = DYN_ALIGNED_VECTOR_TYPE_ARR(nArray);	//create a row of attributes
		for(j=0;j<nArray;j++) context[i][j]=0;
	}

	/* create arrays for object and attribute names */
	onames = new char*[m];
	for (i = 0;i < m;i++) onames[i] = new char[512];
	anames = new char*[n];
	for (i = 0;i < n;i++) anames[i] = new char[512];

	/* get object and attribute names  VS2012*/
	for(i = 0; i < m; i++){
		string s = to_string(i);
		char const *p = s.c_str();
		strcpy_s(onames[i],10, p);
	}
	for(j = 0; j < n; j++){
		string s = to_string(j);
		char const *p = s.c_str();
		strcpy_s(anames[j],10, p);
	}

	/* get object and attribute names VS2010 */
		//char *intStr;
		//for(i = 0; i < m; i++){
		//	itoa(i,intStr,10);
		//	//string s = to_string(i);
		//	string str = string(intStr);
		//	char const *p = str.c_str();
		//	strcpy_s(onames[i],10, p);
		//}
		//for(j = 0; j < n; j++){
		//	//string s = to_string(j);
		//	itoa(j,intStr,10);
		//	string str = string(intStr);
		//	char const *p = str.c_str();
		//	strcpy_s(anames[j],10, p);
	//}

	/* input instances and translate into temporary context */
	datFile.clear();
	datFile.seekg (0, ios::beg);

	int r = 0; // row number
	while(getline(datFile,line)) {

		istringstream s(line);
		string cell;

		while(getline(s, cell, ' ')) {
			vector<string>::iterator k = find(attributes.begin(), attributes.end(), cell);
			if(k != attributes.end()) {
				int j;
				istringstream(cell) >> j;
				contextTemp[(j-1)][(r>>6)] |= (VECTOR_1 <<(r%64));	//set context bit to true where I is byte: i div 8, bit: i mod 8
				colSup[(j-1)]++; //increment column support for attribute j
			}
		}
		r++;
	}

	datFile.close();
}


void cxtFileInput()		//input data from Burmeister cxt file
{
	int i,j;			//object and attribute counters
	ifstream cxtFile;
	cxtFile.open (fname.c_str());

	cout << "\n\nReading data...";
	char Bchar;
	cxtFile >> Bchar;	//strip out the 'B' at top of Burmeister cxt file!
	cxtFile >> m;		//input number of objects
	cxtFile >> n;		//input number of attributes

	/* create temporary context for sorting*/
	mArray = (m-1)/64 + 1;							//calculate size of second dimension (objects) - 1bit per object
	contextTemp = DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(n);				//create one dimension of the temporary context
	for (j = 0;j < n;j++){							//for each attribute
		contextTemp[j] = DYN_ALIGNED_VECTOR_TYPE_ARR(mArray);	//create a row of objects
		for(i=0;i<mArray;i++) contextTemp[j][i]=0;
	}

	/* create context */
	nArray = (n-1)/64 + 1;						//calculate size of second dimension (attributes) - 1bit per object
	context = DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(m);				//create one dimension of the context
	for (i = 0;i < m;i++){						//for each object
		context[i] = DYN_ALIGNED_VECTOR_TYPE_ARR(nArray);		//create a row of attributes
		for(j=0;j<nArray;j++) context[i][j]=0;
	}

	/* strip out blank lines in cxt file */
	char blank[512];
	cxtFile.getline(blank,512);
	cxtFile.getline(blank,512);

	/* create arrays for object and attribute names */
	onames = new char*[m];
	for (i = 0;i < m;i++) onames[i] = new char[512];
	anames = new char*[n];
	for (i = 0;i < n;i++) anames[i] = new char[512];

	/* get object and attribute names */
	for(i = 0; i < m; i++)
		cxtFile.getline(onames[i],512);
	for(j = 0; j < n; j++)
		cxtFile.getline(anames[j],512);

	/* create input row (instance) of context grid to be input from file */
	char *instance;
	int instanceSize = (n+2);
	instance = new char[instanceSize];

	/* input instances and translate into temporary context */
	for(i = 0; i < m; i++){								//for each row (object),
		cxtFile.getline(instance, instanceSize);		//get instance.
		for(j = 0;j < n; j++){							//for each attribute,
			if(instance[j] == 'X'){						//if object has the attibute,
				contextTemp[j][(i>>6)] |= (VECTOR_1<<(i%64));	//set context bit to true where I is byte: i div 8, bit: i mod 8
				colSup[j]++;							//increment column support for attribute j
			}
		}
	}
	cxtFile.close();
}

void cxtFileOutput() {		//output data from Burmeister cxt file

	ofstream sortedcxtfile;
	sortedcxtfile.open("sorted.cxt");
	// Add 'B' followed by number of objects and attributes
	sortedcxtfile << "B" << "\n\n" << m << "\n" << n << "\n\n";

	/* output names of objects and attributes */
	for(int i = 0; i < m; i++){
			sortedcxtfile << onames[rowOriginal[i]] << "\n";
	}

	for(int j = n-1; j >= 0; j--){
			sortedcxtfile << anames[colOriginal[j]] << "\n";
	}

	/* output the sorted context grid*/
	/* note that the (new) context will be column-sorted in dec order	*/
	char *instance;
	int instanceSize = (n+2);
	instance = new char[instanceSize];
	ZeroMemory(instance,sizeof(char)*instanceSize);
	for(int i = 0; i < m; i++){
		int numchars=0;
			for(int j = n-1; j >= 0; j--){
					if(context[i][j>>6]&(VECTOR_1<<(j%64)))
						instance[numchars++] = 'X';
					else
						instance[numchars++] = '.';
				}
			instance[n] = '\n';
			sortedcxtfile << instance;
			}
}

void calcAandBsizes()	//calculate sizes of extents and intents of each concept
{
	int highsizeB = 0;							//largest intent generated
	for(int c = 0; c < highc; c++){				//for each concept
		sizeA[c] = startA[c+1] - startA[c];		//calculate size of extent
		/* calculate size of intent by traversing B tree */
		int i = c;
		while(i >= 0){
			sizeB[c] = sizeB[c] + sizeBnode[i];	//add number of attributes at node i to size
			i = nodeParent[i];					//get the next node (parent)
		}
		if(sizeB[c] > highsizeB) highsizeB = sizeB[c];
		if(m) percentA[c] = (sizeA[c]*100) / m;
	}
	/* add empty infimum if necessary */
	if(highsizeB < n){
		sizeBnode[highc] = n;
		nodeParent[highc] = -1;
		startB[highc] = bptr;
		for(int j = 0; j < n; j++){
			*bptr = j;
			bptr++;
		}
		sizeA[highc] = 0;
		sizeB[highc] = n;
		highc++;
	}
}

void outputNoConsBySize()
{
	/* create arrays to store the number of concepts of each size */
	int *noOfBSize = new int[n+1];
	ZeroMemory(noOfBSize,sizeof(int)*(n+1));
	int *noOfASize = new int[m+1];//[MAX_ROWS];
	ZeroMemory(noOfASize,sizeof(int)*(m+1));

	/* calculate number of concepts of each size satisfying min supports */
	/* calculate number of concepts satisfying min supports              */
	for(int c = 0; c < highc; c++){
		if(sizeB[c] >= minIn && sizeA[c] >= minEx){
			noOfBSize[sizeB[c]]++;
			noOfASize[sizeA[c]]++;
			numcons++;
		}
	}
	if(noOfBSize[n]==0 && minIn==0){
		noOfBSize[n] = 1;
		numcons++;
	}

	/* output list of <size B> - <#concepts> */
	ofstream outBlist;
	outBlist.open ("noConsByBsize.txt", ios_base::out);
	outBlist << "<size B> - <#concepts> min A: " << minEx << " min B: " << minIn << "\n";
	for(int c = minIn; c <= n; c++){
		if(noOfBSize[c] > 0) outBlist << c << " - " << noOfBSize[c] << "\n";
	}
	outBlist.close();

	/* output list of <size A> - <#concepts> */
	ofstream outAlist;
	outAlist.open ("noConsByAsize.txt", ios_base::out);
	outAlist << "<size A> - <#concepts> min A: " << minEx << " min B: " << minIn << "\n";
	for(int i = minEx; i <= m; i++){
		if(noOfASize[i] > 0) outAlist << i << " - " << noOfASize[i] << "\n";
	}
	outAlist.close();
}

void sortColumns()
{
	void colQSort(int colSup[], int colOriginal[], int lo, int high);

	int temp,i,j;
	/* bubble sort column indexes (logical sort) - ascending order of support*/
	for (i = 0 ; i < n ; i++){
		for (j = 0 ;j <  n-i-1; j++){
			if(colSup[j] > colSup[j+1]){
				temp = colSup[j];
				colSup[j] = colSup[j+1];
				colSup[j+1] = temp;
				temp = colOriginal[j];
				colOriginal[j] = colOriginal[j+1];	//keep track of original columns
				colOriginal[j+1] = temp;
			}
		}
	}

	/* rewrite sorted context (physical sort) */
	int tempColNums[MAX_COLS];
	int rank[MAX_COLS];
	for(j = 0; j < n; j++){
		tempColNums[j]=colOriginal[j]; //use original col nos to index the sort
		rank[colOriginal[j]]=j;			//record the ranking of the column
	}
	for(j = 0; j < n - 1; j++){
		for(i = 0; i < mArray; i++){
			VECTOR_TYPE temp = contextTemp[j][i];
			contextTemp[j][i] = contextTemp[tempColNums[j]][i];
			contextTemp[tempColNums[j]][i] = temp;
		}
		tempColNums[rank[j]]=tempColNums[j];		//make note of where swapped-out col has moved to using its rank
		rank[tempColNums[j]]=rank[j];
	}
}

void sortColumnsR()
{
	void colQSort(int colSup[], int colOriginal[], int lo, int high);

	int temp,i,j;
	/* bubble sort column indexes (logical sort) - ascending order of support*/
	for (i = 0 ; i < n ; i++){
		for (j = 0 ;j <  n-i-1; j++){
			if(colSup[j] < colSup[j+1]){
				temp = colSup[j];
				colSup[j] = colSup[j+1];
				colSup[j+1] = temp;
				temp = colOriginal[j];
				colOriginal[j] = colOriginal[j+1];	//keep track of original columns
				colOriginal[j+1] = temp;
			}
		}
	}

	/* rewrite sorted context (physical sort) */
	int tempColNums[MAX_COLS];
	int rank[MAX_COLS];
	for(j = 0; j < n; j++){
		tempColNums[j]=colOriginal[j]; //use original col nos to index the sort
		rank[colOriginal[j]]=j;			//record the ranking of the column
	}
	for(j = 0; j < n - 1; j++){
		for(i = 0; i < mArray; i++){
			VECTOR_TYPE temp = contextTemp[j][i];
			contextTemp[j][i] = contextTemp[tempColNums[j]][i];
			contextTemp[tempColNums[j]][i] = temp;
		}
		tempColNums[rank[j]]=tempColNums[j];		//make note of where swapped-out col has moved to using its rank
		rank[tempColNums[j]]=rank[j];
	}
}

void sortRows()
{

	void quickBent2Hamsort(int left,int right,int original[]);

	quickBent2Hamsort(0, m-1, rowOriginal);

	/* rewrite sorted context (physical sort) */
	int* tempRowNums = new int[MAX_ROWS];
	int* rank = new int[MAX_ROWS];
	//for(int i = 0; i < m; i++){
	for(int i = m-1; i >= 0; i--){
		tempRowNums[i]=rowOriginal[i];	//use original row nos to index the sort
		rank[rowOriginal[i]]=i;			//record the ranking of the row
	}
	//for(int i = 0; i < m-1; i++){
	for(int i = m-1; i >= 0; i--){
		//for(int j = 0; j < nArray; j++){
		for(int j = nArray - 1; j >= 0; j--){
			VECTOR_TYPE temp = context[i][j];
			context[i][j] = context[tempRowNums[i]][j];
			context[tempRowNums[i]][j] = temp;
		}
		tempRowNums[rank[i]]=tempRowNums[i];		//make note of where swapped-out row has moved to using its rank
		rank[tempRowNums[i]]=rank[i];
	}
	delete tempRowNums, rank;
}



void quickBent2Hamsort(int left,int right,int original[])
{
	bool biggerHam(int i1, int i2);
	bool eq(int i1, int i2);
	void exchange(int original[], int from, int to);
	void insertionSort(int left, int right, int original[]);
	int median3(int original[], int i, int j, int k);

	if(right < left) return;

	int n = right - left + 1;

	if(n <= 8) {
		insertionSort(left, right, original);
		return;
	} else if (n <= 60) {
		int m = median3(original, left, left + n/2, right);
		exchange(original, m, left);
	} else {
		  int eps = n/8;
            int mid = left + n/2;
            int m1 = median3(original, left, left + eps, left + eps + eps);
            int m2 = median3(original, mid - eps, mid, mid + eps);
            int m3 = median3(original, right - eps - eps, right - eps, right);
            int ninther = median3(original, m1, m2, m3);
            exchange(original, ninther, left);
	}


	int i = left;
	int j = right + 1;

	int p = left;
	int q = right + 1;

	int pivotIndex = left;
	int pivotValue =  original[pivotIndex];

	while(true) {

		while(biggerHam(original[++i], pivotValue))
			if(i == right)
				break;

		while(biggerHam(pivotValue, original[--j]))
			if(j == left)
				break;

		// pointers cross
		if(i == j && eq(original[i], pivotValue))
			exchange(original, ++p, i);

		if(i >= j)
			break;

		exchange(original, i, j);

		if(eq(original[i], pivotValue))
			exchange(original, ++p, i);
		if(eq(original[j], pivotValue))
			exchange(original, --q, j);
	}


	// exhange equal parts on left and right sides to the middle
	i = j + 1;
	for(int k = left; k <= p; k++)
		exchange(original, k, j--);
	for(int k = right; k >= q; k--)
		exchange(original, i++, k);

	quickBent2Hamsort(left, j, original);
	quickBent2Hamsort(i, right, original);

	}

void insertionSort(int left, int right, int original[]) {

	bool biggerHam(int i1, int i2);
	void exchange(int original[], int left, int right);

	for(int i = left; i <= right; i++) {
		for( int j = i;  j > left && biggerHam(original[j], original[j-1]); j--)
			exchange(original, j, j-1);
	}
}

int median3(int original[], int i, int j, int k) {

	bool biggerHam(int i1, int i2);

	int med;
	biggerHam(original[i], original[j]) ?
               (biggerHam(original[j], original[k]) ? med = j : biggerHam(original[i], original[k]) ? med = k : med = i) :
               (biggerHam(original[k], original[j]) ? med = j : biggerHam(original[k], original[i]) ? med = k : med = i);
	return med;
}



bool biggerHam(int i1, int i2)
{
	//for(int j=0;j<nArray;j++){
	for(int j=nArray-1;j>=0;j--){
		if(context[i1][j]>context[i2][j])return true;
		if(context[i2][j]>context[i1][j])return false;
	}
	return false;
}

bool eq(int i1, int i2)
{
	for(int j=nArray-1;j>= 0;j--){
		if(context[i1][j] != context[i2][j])
			return false;
	}
	return true;
}

void exchange(int original[], int from, int to) {

	int temp = original[from];
	original[from] = original[to];
	original[to] = temp;
}

void outputContext()
{
	cout << "\n\nOutputting reduced context file (only uses concepts that satify minimum support)...";

	/* flags to use to control susequent file output: only output rows and columns that are not empty */
	bool colHasSupport[MAX_COLS];
	bool* rowHasSupport = new bool[MAX_ROWS];
	for(int i=0;i<m;i++) rowHasSupport[i]=false;
	for(int j=0;j<n;j++) colHasSupport[j]=false;

	/* zeroise context */
	for (int i = 0;i < m;i++)
		for(int j = 0;j < nArray; j++)
			context[i][j]=0;

	/* for each concept */
	for(int c = 0; c < highc; c++){
		if(sizeB[c] >= minIn && sizeA[c] >= minEx){				//if concept satisfies min support (is large enough)
			/* traverse B tree to obtain attributes */			//then write it to the new context
			int k = c;											//set i to concept
			while(k >= 0){										//when i==-1, head node is reached
				short int *bptr = startB[k];					//point to start of attributes in first node of this intent
				for(int j = 0; j < sizeBnode[k]; j++){			//iterate attributes at this node
					int b = *bptr;								//obtain original attribute index
					colHasSupport[b]=true;
					bptr++;
					/* obtain extent from A */
					int * aptr = startA[c];						//point to start of extent
					for(int i = 0; i < sizeA[c]; i++){			//iterate objects
						context[*aptr][b>>6] |= (VECTOR_1<<(b%64));	//set context cell to false for this (ob,at) pair
						rowHasSupport[*aptr]=true;
						aptr++;
					}
				}
				k = nodeParent[k];								//point to next node
			}
		}
	}

	/*count new number of objects and attibutes in reduced context*/
	int newm=0;
	int newn=0;
	for(int i=0;i<m;i++) if(rowHasSupport[i]) newm++;
	for(int j=0;j<n;j++) if(colHasSupport[j]) newn++;

	/* output reduced-context Burmeister file */
	FILE *fpsc;
	fopen_s(&fpsc, "context.cxt", "w");

	/* output header: B numobs numats */
	fputs("B\n\n",fpsc);
	char nobs[10], nats[10];
	_itoa_s(newm,nobs,10);
	_itoa_s(newn,nats,10);
	fputs(nobs,fpsc);
	fputs("\n",fpsc);
	fputs(nats,fpsc);
	fputs("\n\n",fpsc);

	/* output names of objects and attributes */
	for(int i = 0; i < m; i++){
		if(rowHasSupport[i]){
			fputs(onames[rowOriginal[i]],fpsc);
			fputs("\n",fpsc);
		}
	}
	//for(int j = 0; j < n; j++){
	for(int j = n-1; j >=0; j--){
		if(colHasSupport[j]){
			fputs(anames[colOriginal[j]],fpsc);
			fputs("\n",fpsc);
		}
	}

	/* output reduced context grid - only output non-empty rows/cols	*/
	/* note that the (new) context will be column-sorted in dec order	*/
	char *instance;
	int instanceSize = (newn+2);
	instance = new char[instanceSize];
	ZeroMemory(instance,sizeof(char)*instanceSize);
	for(int i = 0; i < m; i++){
		int numchars=0;
		if(rowHasSupport[i]){
			//for(int j = 0; j < n; j++){
			for(int j = n-1; j >=0; j--){
				if(colHasSupport[j]){
					if(context[i][j>>6]&(VECTOR_1<<(j%64)))
						instance[numchars++] = 'X';
					else
						instance[numchars++] = '.';
				}
			}
			instance[newn] = '\n';
			fputs(instance,fpsc);
		}
	}
	fclose(fpsc);
}
