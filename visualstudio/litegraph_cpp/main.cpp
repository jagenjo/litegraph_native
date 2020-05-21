
#include <fstream>
#include <iostream>
#include <vector>

#include "../../src/litegraph.h"

//used only for sleep and keypress
#include <windows.h>

int main()
{
    std::cout << "LiteGraph v0.1\n*********************\n"; 

	std::string filename;
	filename = "data/events2.JSON";

	//this will register all the base nodes
	LiteGraph::init();
	
	//register here your own nodes
	//...

	std::cout << "Loading graph..." << filename << std::endl;
	std::string data = LiteGraph::getFileContent( filename.c_str() );
	if (!data.size())
		std::cout << "file not found or empty:" << filename << std::endl;

	LiteGraph::LGraph mygraph;
	if (!mygraph.configure(data))
		exit(1);

	std::cout << "Starting graph execution ****" << std::endl;
	while (1)
	{
		mygraph.runStep(0.01f);
		Sleep(10);
		if ( (GetKeyState(' ') | GetKeyState(27)) & 0x8000 ) /*Check if high-order bit is set (1 << 15)*/
			break;
	}
}
