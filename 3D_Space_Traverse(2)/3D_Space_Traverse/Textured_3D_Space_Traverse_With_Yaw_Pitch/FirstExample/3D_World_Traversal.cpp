

using namespace std;
#include <windows.networking.sockets.h>
#pragma comment(lib, "Ws2_32.lib")

#include "vgl.h"
#include "LoadShaders.h"
#include "glm\glm.hpp"
#include "glm\gtc\matrix_transform.hpp"
#include "glm\gtx\rotate_vector.hpp"
#include "..\SOIL\src\SOIL.h"
#include <vector>
#include <algorithm>
#include "Serialize.h"
#include <iostream> 

/*
TODO: Ask:

How deleting old clients from the server-side scengegraph
why must the scenegraph be interated through and updated one by one
can't I just remove the actor that matches the current Actor's id?
is it alright that I created a const int for the actor's id?


*/



//Used to represent a Actor
struct Actor
{
	int id;	//Unique ID of this actor
	float x_pos, y_pos;	//Position of a Actor
	bool isAlive;	//Still alive?
	bool isBullet;	//Is this actor a bullet or a player?
	bool touchedActor;	//If this Actor is collided with other Actor
	glm::vec2 direction;	//Motion direction
};

//This client's actor's id
const int my_id = 101;

///Networking variables - is it okay I did this?
WSADATA wsaData;
SOCKET clientSocket;
sockaddr_in SvrAddr;


float wheel_rotation = 0.0f;
float Actor_velocity = 3.0f;

//We use a container to keep the data corresponding to Actors
vector<Actor> sceneGraph;

enum VAO_IDs { Triangles, NumVAOs };
enum Buffer_IDs { ArrayBuffer };
enum Attrib_IDs { vPosition = 0 };

const GLint NumBuffers = 2;
GLuint VAOs[NumVAOs];
GLuint Buffers[NumBuffers];
GLuint location;
GLuint cam_mat_location;
GLuint proj_mat_location;
GLuint texture[2];	//Array of pointers to textrure data in VRAM. We use two textures in this example.

const GLuint NumVertices = 28;

//Height of camera (Actor) from the level
float height = 0.8f;

//Actor motion speed for movement and pitch/yaw
float travel_speed = 60.0f;		//Motion speed
float mouse_sensitivity = 0.01f;	//Pitch/Yaw speed

//Used for tracking mouse cursor position on screen
int x0 = 0;
int y_0 = 0;

//Transformation matrices and camera vectors
glm::mat4 model_view;
glm::vec3 unit_z_vector = glm::vec3(0, 0, 1);	//Assigning a meaningful name to (0,0,1) :-)
glm::vec3 cam_pos = glm::vec3(0.0f, 0.0f, height);
glm::vec3 forward_vector = glm::vec3(1, 1, 0);	//Forward vector is parallel to the level at all times (No pitch)

//The direction which the camera is looking, at any instance
glm::vec3 looking_dir_vector = glm::vec3(1, 1, 0);
glm::vec3 up_vector = unit_z_vector;
glm::vec3 side_vector = glm::cross(up_vector, forward_vector);

//Used to measure time between two frames
float oldTimeSinceStart = 0;
float deltaTime;

//Creating and rendering bunch of objects on the scene to interact with
const int Num_Obstacles = 10;
float obstacle_data[Num_Obstacles][3];

//only increase score when the either the bullet or Actor is colliding
bool Increasescore = false;
//score counter
int score = 0;
//flag to know if Actor is alive
bool ActorAlive = true;
//counting time since start to spawn Actors
int oldgluttime = -1;





//Function Signatures
void spawnActor();
void spawnActor(Actor v);
void drawActor(float scale, glm::vec2 direction);
void renderActors();
void update();
void sendToServer();
void serverResponse();

//generates a new Actor, called when F is pressed
void shoot()
{
	Actor v;
	v.x_pos = cam_pos.x;
	v.y_pos = cam_pos.y;
	v.isAlive = TRUE;
	v.isBullet = TRUE;
	v.touchedActor = FALSE;
	v.direction = glm::vec2(forward_vector.x, forward_vector.y);
	sceneGraph.push_back(v);
}


bool Collided(Actor v1, Actor v2)
{
	bool result = false;
	if (v1.isBullet && v2.isBullet) {}
	else if (v1.isAlive && v2.isAlive)
	{
		float x_Dist = abs(v1.x_pos - v2.x_pos);
		float y_Dist = abs(v1.y_pos - v2.y_pos);
		if (x_Dist <= 0.9 && y_Dist <= 0.9) { result = true; }
	}
	return result;
}

//disables the Actors when a collision is sensed
void checkCollision()
{
	for (int i = 0; i < sceneGraph.size(); i++)
	{
		for (int j = 0; j < sceneGraph.size(); j++)
		{
			if (i != j)
			{
				Actor v = sceneGraph[i];
				Actor u = sceneGraph[j];

				if ((v.isAlive && u.isAlive) && ((v.isBullet && !u.isBullet) || (!v.isBullet && u.isBullet)) && Collided(v, u))
				{
					Increasescore = true;
				}
				if (Collided(v, u))
				{
					v.isAlive = false;
					u.isAlive = false;
					sceneGraph[i] = v;
					sceneGraph[j] = u;
				}
				if (Increasescore == true)
				{
					score++;
					Increasescore = false;
					if (score >= 10)
					{
						exit(0);
					}
				}

			}
		}
	}
}


void renderActors() {
	checkCollision();
	for (int i = 0; i < sceneGraph.size(); i++) {
		Actor v = sceneGraph[i];

		if (v.isAlive && !v.isBullet)
		{
			glm::vec2 moving_dir = glm::vec2((0 - v.x_pos), (0 - v.y_pos));

			moving_dir = glm::normalize(moving_dir);

			//The following two lines are commented out to prevent the objects from moving
			//v.x_pos += deltaTime * Actor_velocity * moving_dir.x;
			//v.y_pos += deltaTime * Actor_velocity * moving_dir.y;

			model_view = glm::translate(model_view, glm::vec3(v.x_pos, v.y_pos, 0));
			glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);

			//This line is commented out because, this would set the direction on an actor toward the global origin (0, 0, 0)
			//drawActor(1.0, moving_dir);

			//The actor is faced toward its moving direction
			drawActor(1.0, v.direction);

			model_view = glm::mat4(1.0);
			glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);

			GLfloat x_dist = abs(cam_pos.x - v.x_pos);
			GLfloat y_dist = abs(cam_pos.y - v.y_pos);

			if (x_dist <= 0.5 && y_dist <= 0.5) { v.touchedActor = true; }

			if (ActorAlive && v.touchedActor) { ActorAlive = false; cam_pos.z = 0.1; }

			if (abs(v.x_pos) < 0.1 && abs(v.y_pos) < 0.1) v.isAlive = false;
			sceneGraph[i] = v;
		}


		// when Actor is colliding with a bullet
		else if (v.isAlive && v.isBullet)
		{
			glm::vec2 moving_dir = glm::normalize(v.direction);

			v.x_pos += deltaTime * Actor_velocity * moving_dir.x;
			v.y_pos += deltaTime * Actor_velocity * moving_dir.y;

			model_view = glm::translate(model_view, glm::vec3(v.x_pos, v.y_pos, 0));
			glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);
			drawActor(0.4, moving_dir);
			model_view = glm::mat4(1.0);
			glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);

			sceneGraph[i] = v;
		}
	}
}

//Helper function to generate a random float number within a range
float randomFloat(float a, float b)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}

// inititializing buffers, coordinates, setting up pipeline, etc.
void initializeGraphics(void)
{
	glEnable(GL_DEPTH_TEST);

	//Normalizing all vectors
	up_vector = glm::normalize(up_vector);
	forward_vector = glm::normalize(forward_vector);
	looking_dir_vector = glm::normalize(looking_dir_vector);
	side_vector = glm::normalize(side_vector);

	//Randomizing the position and scale of obstacles
	for (int i = 0; i < Num_Obstacles; i++)
	{
		obstacle_data[i][0] = randomFloat(-50, 50); //X
		obstacle_data[i][1] = randomFloat(-50, 50); //Y
		obstacle_data[i][2] = randomFloat(0.1, 10.0); //Scale
	}

	ShaderInfo shaders[] = {
		{ GL_VERTEX_SHADER, "triangles.vert" },
		{ GL_FRAGMENT_SHADER, "triangles.frag" },
		{ GL_NONE, NULL }
	};

	GLuint program = LoadShaders(shaders);
	glUseProgram(program);	//My Pipeline is set up

	GLfloat vertices[NumVertices][3] = {

		{ -100.0, -100.0, 0.0 }, //Plane to walk on and a sky
		{ 100.0, -100.0, 0.0 },
		{ 100.0, 100.0, 0.0 },
		{ -100.0, 100.0, 0.0 },

		{ -0.45, -0.45 ,-0.45 }, // bottom face
		{ 0.45, -0.45 ,-0.45 },
		{ 0.45, 0.45 ,-0.45 },
		{ -0.45, 0.45 ,-0.45 },

		{ -0.45, -0.45 ,0.45 }, //top face
		{ 0.45, -0.45 ,0.45 },
		{ 0.45, 0.45 ,0.45 },
		{ -0.45, 0.45 ,0.45 },

		{ 0.45, -0.45 , -0.45 }, //left face
		{ 0.45, 0.45 , -0.45 },
		{ 0.45, 0.45 ,0.45 },
		{ 0.45, -0.45 ,0.45 },

		{ -0.45, -0.45, -0.45 }, //right face
		{ -0.45, 0.45 , -0.45 },
		{ -0.45, 0.45 ,0.45 },
		{ -0.45, -0.45 ,0.45 },

		{ -0.45, 0.45 , -0.45 }, //front face
		{ 0.45, 0.45 , -0.45 },
		{ 0.45, 0.45 ,0.45 },
		{ -0.45, 0.45 ,0.45 },

		{ -0.45, -0.45 , -0.45 }, //back face
		{ 0.45, -0.45 , -0.45 },
		{ 0.45, -0.45 ,0.45 },
		{ -0.45, -0.45 ,0.45 },
	};

	//These are the texture coordinates for the second texture
	GLfloat textureCoordinates[28][2] = {
		0.0f, 0.0f,
		200.0f, 0.0f,
		200.0f, 200.0f,
		0.0f, 200.0f,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
	};

	//Creating our texture:
	//This texture is loaded from file. To do this, we use the SOIL (Simple OpenGL Imaging Library) library.
	//When using the SOIL_load_image() function, make sure the you are using correct patrameters, or else, your image will NOT be loaded properly, or will not be loaded at all.
	GLint width1, height1;
	unsigned char* textureData1 = SOIL_load_image("grass.png", &width1, &height1, 0, SOIL_LOAD_RGB);

	GLint width2, height2;
	unsigned char* textureData2 = SOIL_load_image("apple.png", &width2, &height2, 0, SOIL_LOAD_RGB);

	glGenBuffers(2, Buffers);
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindAttribLocation(program, 0, "vPosition");
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, Buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(textureCoordinates), textureCoordinates, GL_STATIC_DRAW);
	glBindAttribLocation(program, 1, "vTexCoord");
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(1);

	location = glGetUniformLocation(program, "model_matrix");
	cam_mat_location = glGetUniformLocation(program, "camera_matrix");
	proj_mat_location = glGetUniformLocation(program, "projection_matrix");

	///////////////////////TEXTURE SET UP////////////////////////

	//Allocating two buffers in VRAM
	glGenTextures(2, texture);

	//First Texture: 
	//Set the type of the allocated buffer as "TEXTURE_2D"
	glBindTexture(GL_TEXTURE_2D, texture[0]);

	//Loading the second texture into the second allocated buffer:
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width1, height1, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData1);

	//Setting up parameters for the texture that recently pushed into VRAM
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//And now, second texture: 
	//Set the type of the allocated buffer as "TEXTURE_2D"
	glBindTexture(GL_TEXTURE_2D, texture[1]);

	//Loading the second texture into the second allocated buffer:
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width2, height2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData2);

	//Setting up parameters for the texture that recently pushed into VRAM
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//////////////////////////////////////////////////////////////
}

//Draws the actor on the scene. 
//This function does actually draw the graphics.
void drawActor(float scale, glm::vec2 direction)
{
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	model_view = glm::translate(model_view, glm::vec3(0.0f, 0.0f, 0.45f));
	model_view = glm::rotate(model_view, atan(direction.y / direction.x), unit_z_vector);
	model_view = glm::scale(model_view, glm::vec3(scale, scale, scale));
	glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);
	glDrawArrays(GL_QUADS, 4, 24);
}

//Adds an actor to the scene graph (the vector). The location of this actor is random. 
//Note: This function does NOT draw the actor on the scene.
void spawnActor()
{
	Actor v;
	v.x_pos = randomFloat(-50, 50);
	v.y_pos = randomFloat(-50, 50);
	v.isAlive = TRUE;
	v.touchedActor = FALSE;
	v.isBullet = FALSE;
	sceneGraph.push_back(v);
}

//Adds an actor to the scene graph (the vector).
//Note: This function does NOT draw the actor on the scene, instead, it adds the actor to the scenegraph
void spawnActor(Actor v)
{
	sceneGraph.push_back(v);
}

// display function
void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	model_view = glm::mat4(1.0);
	glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);

	//The 3D point in space that the camera is looking
	glm::vec3 look_at = cam_pos + looking_dir_vector;

	glm::mat4 camera_matrix = glm::lookAt(cam_pos, look_at, up_vector);
	glUniformMatrix4fv(cam_mat_location, 1, GL_FALSE, &camera_matrix[0][0]);

	glm::mat4 proj_matrix = glm::frustum(-0.01f, +0.01f, -0.01f, +0.01f, 0.01f, 100.0f);
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, &proj_matrix[0][0]);

	//Select the first texture (grass.png) when drawing the first geometry (floor)
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glDrawArrays(GL_QUADS, 0, 4);

	//Draw a column in the middle of the scene (0, 0, 0)
	//*****************************************************
	model_view = glm::scale(model_view, glm::vec3(0.1, 0.1, 10));
	glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);
	glDrawArrays(GL_QUADS, 4, 24);
	model_view = glm::mat4(1.0);
	glUniformMatrix4fv(location, 1, GL_FALSE, &model_view[0][0]);
	//*****************************************************

	renderActors();

	glFlush();
}


void keyboard(unsigned char key, int x, int y)
{
	if (ActorAlive)
	{
		if (key == 'a')
		{
			//Moving camera along opposit direction of side vector
			cam_pos += side_vector * travel_speed * deltaTime;
		}
		if (key == 'd')
		{
			//Moving camera along side vector
			cam_pos -= side_vector * travel_speed * deltaTime;
		}
		if (key == 'w')
		{
			//Moving camera along forward vector. To be more realistic, we use X=V.T equation in physics
			cam_pos += forward_vector * travel_speed * deltaTime;
		}
		if (key == 's')
		{
			//Moving camera along backward (negative forward) vector. To be more realistic, we use X=V.T equation in physics
			cam_pos -= forward_vector * travel_speed * deltaTime;
		}
		//press F to shoot Actor
		if (key == 'f')
		{
			shoot();
		}
	}
}

//Controlling Pitch with vertical mouse movement
void mouse(int x, int y)
{
	if (ActorAlive)
	{
		int delta_x = x - x0;

		forward_vector = glm::rotate(forward_vector, -delta_x * mouse_sensitivity, unit_z_vector);

		looking_dir_vector = glm::rotate(looking_dir_vector, -delta_x * mouse_sensitivity, unit_z_vector);

		side_vector = glm::rotate(side_vector, -delta_x * mouse_sensitivity, unit_z_vector);

		up_vector = glm::rotate(up_vector, -delta_x * mouse_sensitivity, unit_z_vector);

		x0 = x;

		int delta_y = y - y_0;

		glm::vec3 tmp_up_vec = glm::rotate(up_vector, delta_y * mouse_sensitivity, side_vector);

		glm::vec3 tmp_looking_dir = glm::rotate(looking_dir_vector, delta_y * mouse_sensitivity, side_vector);

		GLfloat dot_product = glm::dot(tmp_looking_dir, forward_vector);

		if (dot_product > 0)
		{
			up_vector = glm::rotate(up_vector, delta_y * mouse_sensitivity, side_vector);
			looking_dir_vector = glm::rotate(looking_dir_vector, delta_y * mouse_sensitivity, side_vector);
		}

		y_0 = y;
	}
}

void idle()
{
	//Calculate the delta time between two frames
	wheel_rotation += 0.01f;
	float timeSinceStart = (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

	//create Actor at every 2 seconds
	/*if ((int)timeSinceStart % 2 == 0 && oldgluttime != (int)timeSinceStart)
	{
		spawnActor();
		oldgluttime = (int)timeSinceStart;
	}*/
	deltaTime = timeSinceStart - oldTimeSinceStart;
	oldTimeSinceStart = timeSinceStart;
	update();
	glutPostRedisplay();
}


///Start here
/*************************WARNING!!!!!**************************/
/*************************WARNING!!!!!**************************/
/***************DO NOT TOUCH THE CODE ABOVE*********************/
/***************DO NOT TOUCH THE CODE ABOVE*********************/
/*************************WARNING!!!!!**************************/
/*************************WARNING!!!!!**************************/


/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/
/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/
/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/
/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ YOUR CODE GOES BELOW ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/
/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/
/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/
/*↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓*/

/***************************************************************/
/***************************************************************/
/**************************Networking Code**********************/
/***************************************************************/
/***************************************************************/


//Returns the scene-graph
vector<Actor> getSceneGraph() {
	return sceneGraph;
}

void initializeNetwork()
{

	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		std::cout << "Error" << std::endl;
	}

	//create Socket
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		WSACleanup();
		std::cout << "Invalid socket!" << std::endl;
	}

	//connect socket
	SvrAddr.sin_family = AF_INET;						//Address family type itnernet
	SvrAddr.sin_port = htons(27000);					//port (host to network conversion)
	SvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");	//IP address

	if ((connect(clientSocket, (struct sockaddr*) & SvrAddr, sizeof(SvrAddr))) == SOCKET_ERROR) {
		closesocket(clientSocket);
		WSACleanup();
		cout << "Connecting to server failed!!!\n";
	}


	//where to close the socket? -R



}


//This function gets called for every frame, say 30 times per second
void update()
{
	//To be added by astudents
	sendToServer();//send 
	serverResponse();//receive from server


}

//The following function is used to emulate a server
//later on, this function will be used to receive data from a server
void serverResponse()
{
	//Receive the updated scene-graph from server.
	vector<Actor> updatedsceneGraph;
	vector<Actor> clientsceneGraph = getSceneGraph(); //why use getSceneGraph when we can call it directly? -R

	//Update the scene-graph on client side
	//Notice that what you will see in the game is the visual representation of the scene graph
	//So, for every frame of the game, we need to update it based on the data we receive from the server
	//Steps to follow for every frame:
	//1 - Call getSceneGraph() function that returns the scene-graph of client-side
	//2 - On the other hand, you receive the a scene graph from the sever 
	//3 - You will need to iterate through the updated scene graph received from the server and update the client-side scene-graph, one by one.
	//Note: The scene graph on the client side only contains the data for other clients, but not the data for itself. This is illustrated in lab 7 in more details in the instruction file
	//Finally, keep in mind that the game represents a snap-shot of the game-scene.
	int num_actors = 0;
	char rvBuffer[9001];
	int n = recv(clientSocket, rvBuffer, sizeof(rvBuffer), 0);

	if (n > 0) {
		num_actors = (n / sizeof(Actor));

		char* buff = (char*)malloc(sizeof(Actor) * num_actors);
		buff = rvBuffer;

		Actor* act = (Actor*)buff;

		//create the updated scenegraph 
		for (int i = 0; i < num_actors; ++i) {
			Actor a = (*act);
			act++;
			updatedsceneGraph.push_back(a);
		}


		auto it = std::find_if(updatedsceneGraph.begin(), updatedsceneGraph.end(), [&](Actor c) {return c.id == my_id; });
		if (it != updatedsceneGraph.end()) {
			updatedsceneGraph.erase(it);
		}
		sceneGraph = updatedsceneGraph;

	}
	else {
		std::cout << "Receive failed!!" << std::endl;
	}





}

void sendToServer()
{
	//Create an instance of myself and send to server
	Actor v;
	v.x_pos = cam_pos.x;
	v.y_pos = cam_pos.y;

	if (ActorAlive) {	//ActorAlive is a flag that shows if I am still alive
		v.isAlive = true;
	}
	else {
		v.isAlive = FALSE;
	}
	v.touchedActor = FALSE;
	v.isBullet = FALSE;
	//Perhaps I can make a macro to hold this value so I can use it when updating the scenegraph ?
	//for now I'm using a const int-R
	v.id = my_id;						//Note: This must be a unique number for each client
	v.direction = glm::vec2(forward_vector.x, forward_vector.y);

	char* t = convert(v);
	send(clientSocket, t, sizeof(Actor), 0);

}



// main
int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA);
	glutInitWindowSize(1024, 1024);
	glutCreateWindow("Camera and Projection");
	glewInit();	//Initializes the glew and prepares the drawing pipeline.

	initializeGraphics();

	glutDisplayFunc(display);

	glutKeyboardFunc(keyboard);

	glutIdleFunc(idle);

	glutPassiveMotionFunc(mouse);

	initializeNetwork();

	glutMainLoop();

	closesocket(clientSocket);
	WSACleanup();
}

