#pragma once

#include "../litegraph.h"
using namespace LiteGraph;

class OutputNode : public LGraphNode
{
public:
	REGISTERNODE("graph/output", OutputNode);

	OutputNode();

	void onExecute()
	{
	}
};

class WatchNode : public LGraphNode
{
public:
	REGISTERNODE( "basic/watch", WatchNode );

	WatchNode();
	void onExecute();
};

class ConsoleNode : public LGraphNode
{
public:
	REGISTERNODE("basic/console", ConsoleNode);

	ConsoleNode();
	void onExecute();
	void onAction(int slot, LEvent& event);
};

class TimeNode : public LGraphNode
{
public:
	REGISTERNODE("basic/time", TimeNode);

	TimeNode();
	void onExecute();
};

// *********************

class ConstNumberNode : public LGraphNode
{
public:
	REGISTERNODE("basic/const", ConstNumberNode);

	double value;

	ConstNumberNode();
	void onExecute();
	void onConfigure(void* json);
};

class ConstStringNode : public LGraphNode
{
public:
	REGISTERNODE("basic/string", ConstStringNode);

	std::string value;

	ConstStringNode();
	void onExecute();
	void onConfigure(void* json);
};

class ConstDataNode : public LGraphNode
{
public:
	REGISTERNODE("basic/data", ConstDataNode);

	JSON value;

	ConstDataNode();
	virtual ~ConstDataNode();
	void onExecute();
	void onConfigure(void* json);
};


class ObjectPropertyNode : public LGraphNode
{
public:
	REGISTERNODE("basic/object_property", ObjectPropertyNode);

	std::string name;
	std::string _str;

	ObjectPropertyNode();
	void onExecute();
	void onConfigure(void* json);
};

// *********************

class GateNode : public LGraphNode
{
public:
	REGISTERNODE("math/gate", GateNode);
	GateNode();
	void onExecute();
};

// Math *********************

class ConditionNode : public LGraphNode
{
public:
	REGISTERNODE("math/condition", ConditionNode);

	enum ConditionType {
		EQUAL,
		NEQUAL,
		GREATER,
		GEQUAL,
		LESS,
		LEQUAL,
		OR,
		AND
	};

	double A;
	double B;
	ConditionType OP;

	ConditionNode();
	void onExecute();
	void onConfigure(void* json);
};

class TrigonometryNode : public LGraphNode
{
public:
	REGISTERNODE("math/trigonometry", TrigonometryNode);

	double amplitude;
	double offset;

	TrigonometryNode();
	void onExecute();
	void onConfigure(void* json);
};


// Events **********************************

class TimerNode : public LGraphNode
{
public:
	REGISTERNODE("events/timer", TimerNode);

	double interval; //in ms
	double _next_trigger;

	TimerNode();
	void onExecute();
	void onConfigure(void* json);
};