#include "Cubing.h"

Cubing::Cubing()
{
	dimsNum = 0;
	msrsNum = 0;
	tuplesNum = 0;
	data = NULL;
	msrdata = NULL;

	delimiter = " ";
	cubeDir = "";
	tstart = 0;

	// 杨卓荦更正调用不规范
	aggFunLib[0] = &Cubing::sumFun;
	aggFunLib[1] = &Cubing::countFun;
	aggFunLib[2] = &Cubing::avgFun;
	aggFunLib[3] = &Cubing::maxFun;
	aggFunLib[4] = &Cubing::minFun;
}

Cubing::~Cubing()
{
}

/*
  load data from a data file. using array instead of vector will decrease 
  sharply the amount of time because dynamically allocating memory will 
  cause the many data movement. e.g. time consuming drops from 13s to 4.6s.
*/
void Cubing::loadData(string dataFileName)
{
	//get cube dir
	string::size_type pos = dataFileName.find(".txt");
	if(pos != string::npos)
		cubeDir = dataFileName.substr(0, pos);
	mkdir(cubeDir.c_str());
	/*
	if(mkdir(cubeDir.c_str()) != 0)  //if exist the file or folder having the same name
	{
		cout<<"can't create the directiory";
		exit(-1);
	}
	*/

	// open data file
	DFHandle df;
	if (!df.Open(dataFileName.c_str())) //判断文件是否open
	{
		cout<<"Error: cannot open file "<<dataFileName<<"."<<endl;
		exit(-1);
	}

	//TID tid;
	FID fid;
	char field[MAXTUPLELEN];
	TupleHandle tHandle(delimiter);

	df.GetNextTuple(tHandle);
	while(tHandle.GetNextField(field, sizeof(field), fid))
	{
		string tmp = field;
		int pos = tmp.find("measures.");
		if(pos < 0)
		{
			dimsNum ++;
		}
		else
		{
			int pos1 = tmp.rfind(",");
			string aggFunType = tmp.substr(pos1 + 1);
			if(aggFunType == "sum")
				aggFunOrder.push_back(0);
			else if(aggFunType == "count")
				aggFunOrder.push_back(1);
			else if(aggFunType == "avg")
				aggFunOrder.push_back(2);
			else if(aggFunType == "max")
				aggFunOrder.push_back(3);
			else if(aggFunType == "min")
				aggFunOrder.push_back(4);
			else
			{
				//sum aggregate by default
				aggFunOrder.push_back(0);
			}
			msrsNum ++;
		}
	}

	//count tuples
	//df.ReWind();
	while(df.GetNextTuple(tHandle))
	{
		tuplesNum ++;
	}

	int i, j, k;

	data = new int* [tuplesNum];
	for(i = 0; i < tuplesNum; i++)
	{
		data[i] = new int[dimsNum];//S data:[[1,1,1],[1,2,1],[2,1,2]]的矩阵
	}

	msrdata = new float* [tuplesNum];
	for(i = 0; i < tuplesNum; i++)
	{
		msrdata[i] = new float[msrsNum];
	}

	#ifdef MAPPER

	Mapper **mapper = new Mapper* [dimsNum]; // 我认为这个地方可以改为Mapper一维数组或者Mapper向量
	for(i = 0; i < dimsNum; i++)
	{
		mapper[i] = new Mapper;
	}

	int *card = new int[dimsNum]; // 我认为这个地方应该使用memset或者 vector<int> card(dimsNum, 0) 以提高效率
	for(i = 0; i < dimsNum; i++)
	{
		card[i] = 0;
	}
	
	//open map file
	FILE **fpsMap = new FILE* [dimsNum];
	for(i = 0; i < dimsNum; i ++)
	{
		string mapFileName = cubeDir + "\\map" + itos(i) + ".txt";
		fpsMap[i] = fopen(mapFileName.c_str(), "w");
		if(fpsMap[i] == NULL)
		{
			cout<<"can't open map files."<<endl;
			exit(-1);
		}
	}
	
	i = 0;
	df.ReWind();
	df.GetNextTuple(tHandle);  //skip the header line
	while(df.GetNextTuple(tHandle))
	{
		j = 0;
		k = 0;
		//tHandle.GetTid(tid);
		while(tHandle.GetNextField(field, sizeof(field), fid)) // 得到了一个field
		{
			if(j < dimsNum)
			{
				//find field in the map table
				Mapper::iterator lb = mapper[j]->lower_bound(field); // lower_bound得到某一个键的下界
				if(lb != mapper[j]->end() && !(mapper[j]->key_comp()(field, lb->first))) // 如果lb存在，且当前field与下界的键lp->first不等
				{
					data[i][j] = lb->second;
				}
				else
				{
					card[j] ++;
					mapper[j]->insert(lb, Mapper::value_type(field, card[j]));
					data[i][j] = card[j];

					//string mapperData = string(field) + " " + itos(card[j]) + "\n";
					string mapperData = string(field) + "\n";
					fwrite(mapperData.c_str(), strlen(mapperData.c_str()), 1, fpsMap[j]);
				}
				j ++;
			}
			else if(k < msrsNum)
			{
				msrdata[i][k] = atof(field);
				k ++;
			}
		}
		i ++;
	}

	cout << "dimsNum=" << dimsNum << endl;//S 测试
	cout << "msrsNum=" << msrsNum << endl;//S 测试
	cout << "tuplesNum=" << tuplesNum << endl;//S 测试


	for(i = 0; i < dimsNum; i ++)
	{
		delete mapper[i];
	}
	delete[] mapper;
	delete[] card;

	if(fpsMap)
	{
		for(int i = 0; i < dimsNum; i ++)
		{
			fclose(fpsMap[i]);
		}
		delete[] fpsMap;
	}

	#else

	i = 0;
	df.ReWind();
	df.GetNextTuple(tHandle);  //skip the header line
	while(df.GetNextTuple(tHandle))
	{
		j = 0;
		k = 0;
		//tHandle.GetTid(tid);
		while(tHandle.GetNextField(field, sizeof(field), fid))
		{
			if(j < dimsNum)
			{
				data[i][j] = atoi(field);
				j ++;
			}
			else if(k < msrsNum)
			{
				msrdata[i][k] = atof(field);
				k ++;
			}
		}
		i ++;
	}

	#endif

	df.Close();
}

void Cubing::preCompute()
{
}

int Cubing::getAggsNum()
{
	return 0;
}

float Cubing::sumFun(float **msrdata, int bPos, int ePos, int msrNum)
{
	float value = 0;
	for(int i = bPos; i < ePos; i++)
	{
		value += msrdata[i][msrNum]; //度量值总和sum
	}
	
	return value;
}

float Cubing::countFun(float **msrdata, int bPos, int ePos, int msrNum)
{
	return 0;
}

float Cubing::avgFun(float **msrdata, int bPos, int ePos, int msrNum)
{
	return 0;
}

float Cubing::maxFun(float **msrdata, int bPos, int ePos, int msrNum)
{
	/*
	float value = 0;
	for(int i = 0; i < size; i++)
	{
		value > data[i][msrNum] ? value : data[i][msrNum];
	}
	
	return value;
	*/
	return 0;
}

float Cubing::minFun(float **msrdata, int bPos, int ePos, int msrNum)
{
	/*
	float value = 0;
	for(int i = 0; i < size; i++)
	{
		value < data[i][msrNum] ? value : data[i][msrNum];
	}
	
	return value;
	*/
	return 0;
}

void Cubing::beginTimer(string message)
{
	tstart = clock();
	cout<<message<<endl;
}

void Cubing::endTimer(string message)
{
	double endTime = (double)((double)clock() - (double)tstart)/
		(double)CLOCKS_PER_SEC;
	cout<<message<<" "<<endTime<<"s."<<endl;
}
