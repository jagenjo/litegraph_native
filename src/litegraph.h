#pragma once

#include <vector>
#include <string>
#include <map>

namespace LiteGraph {

	struct vec2 { float x; float y;	};
	struct vec3 { float x; float y; float z; };
	struct vec4 { float x; float y; float z; float w; };
	struct mat4 { float m[16]; };

	class LSlot;
	class LLink;
	class LGraph;
	class LGraphNode;

	typedef void* JSON;

	static std::map<std::string, LGraphNode*> node_types;
	void registerNodeType(LGraphNode* node);
	LGraphNode* createNode(const char* name);
	
	enum DataType {
		NONE,
		CUSTOM,
		ENUM,
		NUMBER,
		STRING,
		BOOL,
		VEC2,
		VEC3,
		MAT4,
		JSON_OBJECT,
		OBJECT, //generic object that must be stored inside
		POINTER,//pointer to something outside
		EVENT,//event
		ANY
	};

	//used when triggering events
	class LEvent {
	public:
		std::string type;
		std::string data;
		LEvent() {}
		LEvent(std::string type) { this->type = type; }
		LEvent(std::string type, std::string data) { this->type = type; this->data = data;  }
	};

	void registerCustomDataType(const char* name, int id);
	DataType stringToType(const char* str);

	class LData {
	public:
		DataType type;

		//generic types (this ones cannot have pointers inside)
		union {
			bool boolean;
			uint8_t enumeration;
			double number;
			vec2 vector2;
			vec3 vector3;
			vec4 vector4;
			mat4 matrix44;
		};

		//in case its an event
		LEvent event; 

		//custom
		int bytes; //allocated bytes
		void* custom_data; //generic pointer

		LData();
		~LData();
		void clear();
		void setType(DataType type);
		void setString(const char* str);
		void setPointer(void* obj);
		std::string getString();
		//template<class T> void setObject(T obj);

	};

	class LLink {
	public:
		int id;
		int origin_id;
		int origin_slot;
		int target_id;
		int target_slot;

		LLink(int id, int origin_id, int origin_slot, int target_id, int target_slot);
	};

	class LSlot {
	public:

		std::string name;
		DataType type;
		LData* data; //pointer to the slot data
		LGraphNode* node; //parent node

		LLink* link; //for input slots
		std::vector<LLink*> links; //for output slots

		LSlot(LGraphNode* node, const char* name, DataType type)
		{
			this->type = type;
			this->name = name;
			data = NULL;
			link = NULL;
			this->node = node;
		}

		bool isConnected() { return link != NULL || links.size(); }
		LData* getOriginData();
	};

	#define REGISTERNODE(NODE_NAME,NODE_CLASS) \
		LiteGraph::LGraphNode* clone() { return new NODE_CLASS(); } \
		const char* getType() { return NODE_NAME; } \
		bool mustRegister() { static bool must = true; if(!must) return false; must = false; return true; }


	#define CTOR_NODE() 	if (mustRegister())	LiteGraph::registerNodeType(this);

	class LGraphNode {
	public:
		int id;

		LGraph* graph;
		int order;

		vec2 position;
		vec2 size;

		std::vector<LSlot*> inputs;
		std::vector<LSlot*> outputs;

		LGraphNode();
		virtual void onExecute() {};

		LSlot* addInput(const char* name, DataType type);
		LSlot* addOutput(const char* name, DataType type);

		LSlot* getInputSlot(int i);
		LSlot* getOutputSlot(int i);

		bool isInputConnected(int index);
		bool isOutputConnected(int index);

		//void connect(int output_slot, LGraphNode* target_node, int target_input_slot);

		bool getInputDataAsBoolean(int slot);
		double getInputDataAsNumber(int slot);
		std::string getInputDataAsString(int slot);
		JSON getInputDataAsJSON(int slot);

		void setOutputDataAsBoolean(int slot, bool v);
		void setOutputDataAsNumber(int slot, double v);
		void setOutputDataAsString(int slot, const char* v);
		void setOutputDataAsPointer(int slot, void* v);
		void setOutputDataAsEvent(int slot, LEvent event);

		void trigger(int slot, LEvent event);
		virtual void onAction(int slot, LEvent& event) {};

		virtual const char* getType() { return ""; }
		virtual LGraphNode* clone() { return new LGraphNode();  }; //must use REGISTERNODE( type, classname )
		virtual bool mustRegister() { return false; }

		virtual void configure(void* json_object);
		virtual void serialize(); //not very necessary right now

		virtual void onConfigure(void* json) {}

		void removeSlots();
	};

	class LGraph {
	public:

		std::vector<LGraphNode*> nodes;
		std::map<int,LGraphNode*> nodes_by_id;
		std::vector<LLink*> links;
		std::map<int, LLink*> links_by_id;

		std::vector<LGraphNode*> nodes_in_execution_order;

		std::map<std::string, LData*> outputs;

		int last_node_id;
		int last_link_id;

		bool has_errors;

		double time; //in seconds

		LGraph();
		void clear();

		void add(LGraphNode* node);
		LGraphNode* getNodeById(int id);

		void runStep(float dt = 0);

		bool configure(std::string data);

		void sortByExecutionOrder();

		void setOutput(std::string name, LData* data);
	};

	std::string getFileContent(const std::string& path);

	//wrapper for the JSON parser
	bool readJSONBoolean(JSON obj, const char* name, bool& dst);
	bool readJSONNumber(JSON obj, const char* name, int& dst);
	bool readJSONNumber(JSON obj, const char* name, float& dst);
	bool readJSONNumber(JSON obj, const char* name, double& dst);
	bool readJSONString(JSON obj, const char* name, std::string& dst);
	JSON getJSONObject(JSON obj, const char* name);
	JSON parseJSON(const char* str);
	void freeJSON(JSON obj);
}