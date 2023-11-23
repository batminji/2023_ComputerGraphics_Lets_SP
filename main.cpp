#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <random>
#include <vector>
#include <math.h>
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define WINDOWX 800
#define WINDOWY 800

using namespace std;
random_device rd;
default_random_engine dre(rd());
uniform_real_distribution<float>uid(0, 1);
uniform_real_distribution<float>uid_lean(0.2, 1);
uniform_real_distribution<float>uid_particle(-0.05, 0.05);
uniform_int_distribution<int>uidd(0, 3);
uniform_int_distribution<int>uid_x(0, 1);

void make_vertexShaders();
void make_fragmentShaders();
void make_shaderProgram();
GLvoid drawScene();
GLvoid Reshape(int w, int h);

GLchar* vertexSource, * fragmentSource; //--- 소스코드 저장 변수
GLuint vertexShader, fragmentShader; //--- 세이더 객체
GLuint shaderProgramID;
GLuint VAO, VBO[2];
glm::mat4 TR = glm::mat4(1.0f);
glm::vec4 transformedVertex;
glm::vec3 finalVertex;

struct Point {
    glm::vec3 xyz;
};
Point p;
struct Color {
    GLfloat r, g, b;
};
Color c;

struct Shape {
    int vertex_num; // 도형 꼭짓점 갯수
    int s; // 삼각형으로 만들지 사각형으로 만들지
    vector<Point>point; // 꼭짓점 벡터
    vector<Color>color; // 도형 색
    float x, y; // 도형 중심 좌표 --> 쪼개지면 의미 X
    float tx, ty; // 도형이 이동할 거리
    float rotate_r; // 자전 각도
    float lean; // 이동하는 경로 기울기
    bool split; // 쪼개지면 true --> 그려지지 않음
    bool on_box; // 바구니에 담기면 true --> 바구니를 따라감
    bool draw_route; // 경로 출력해야 하는 도형

    void bind();
    void draw();
};
void Shape::bind() {
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, point.size() * sizeof(Point), point.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, color.size() * sizeof(Color), color.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
}
bool shape_mode = true;
bool route_mode = false;
void Shape::draw() {
    glBindVertexArray(VAO);
    if (!shape_mode) {
        glLineWidth(3.0f);
        glPolygonMode(GL_FRONT, GL_LINE);
        glDrawArrays(GL_TRIANGLE_FAN, 0, point.size());
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLE_FAN, 0, point.size());
    }
}

struct Particle {
    vector<Point>point;
    vector<Color>color;
    float x, y;
    float tx, ty;
    float rotate_r;
    float scale;
    void bind();
    void draw();
};
void Particle::bind() {
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, point.size() * sizeof(Point), point.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, color.size() * sizeof(Color), color.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
}
void Particle::draw() {
    glBindVertexArray(VAO);
    glLineWidth(1.0f);
    glPolygonMode(GL_FRONT, GL_LINE);
    glDrawArrays(GL_TRIANGLE_FAN, 0, point.size());
}
vector<Shape>shape;
vector<Particle>particle;
Shape temp_shape;
Particle temp_particle;
bool mouse_click = false;
int timer_speed = 30;

GLfloat line[2][3];
GLfloat route_line[2][3];
GLfloat line_color[] = {
    1.0, 0.0, 0.0,
    1.0, 0.0, 0.0
};

GLfloat box[4][3] = {
    {-0.2f, -0.85f, 1.0f},
    {-0.2f, -0.95f, 1.0f},
    {0.2f, -0.95f, 1.0f},
    {0.2f, -0.85f, 1.0f}
};

GLfloat box_color[4][3] = {
    {0.0f, 0.0f, 0.8f},
    {0.0f, 0.0f, 0.8f},
    {0.0f, 0.0f, 0.8f},
    {0.0f, 0.0f, 0.8f}
};
float box_xmove = 0.01f;
float box_x = 0.0f;

vector<Point>cross_point;
vector<Point>left_shape;
vector<Point>right_shape;

float X1, X2, X3, X4;
float Y1, Y2, Y3, Y4;
float m1, m2;
int cross_num = 0;

bool crossing_vertex() {
    float cx, cy;
    m1 = (Y2 - Y1) / (X2 - X1); m2 = (Y4 - Y3) / (X4 - X3);
    if (m1 == m2)return false;
    cx = double((m1 * X1 - m2 * X3 - Y1 + Y3) / (m1 - m2));
    cy = double(m1 * cx - m1 * X1 + Y1);
    if (((cy >= Y3 && cy <= Y4) || (cy <= Y3 && cy >= Y4))
        && ((cy >= Y1 && cy <= Y2) ||(cy <= Y1 && cy >= Y2))) { // 교차 !
        p.xyz = { cx, cy, 1.0 };
        cross_point.push_back(p);
        return true;
    }
    return false;
}

void Collide_shape(Shape s) {
    cross_num = 0;
    cross_point.clear();
    left_shape.clear(); right_shape.clear();
    X1 = line[0][0]; Y1 = line[0][1];
    X2 = line[1][0]; Y2 = line[1][1];

    for (int i = 0; i < s.point.size(); ++i) {
        X3 = s.point[i].xyz.x; Y3 = s.point[i].xyz.y;
        if (i == s.point.size() - 1) {
            X4 = s.point[0].xyz.x; Y4 = s.point[0].xyz.y;
        }
        else {
            X4 = s.point[i + 1].xyz.x; Y4 = s.point[i + 1].xyz.y;
        }

        if (crossing_vertex()) {
            if (cross_num == 0) {
                p.xyz = { s.point[i].xyz.x, s.point[i].xyz.y, 1.0 };
                left_shape.push_back(p);
                p.xyz = cross_point[0].xyz;
                left_shape.push_back(p); right_shape.push_back(p);
            }
            else if (cross_num == 1) {
                p.xyz = { s.point[i].xyz.x, s.point[i].xyz.y, 1.0 };
                right_shape.push_back(p);
                p.xyz = cross_point[1].xyz;
                left_shape.push_back(p); right_shape.push_back(p);
            }
            cross_num++;
        }
        else {
            if (cross_num == 0) {
                p.xyz = { s.point[i].xyz.x, s.point[i].xyz.y, 1.0 };
                left_shape.push_back(p);
            }
            else if (cross_num == 1) {
                p.xyz = { s.point[i].xyz.x, s.point[i].xyz.y, 1.0 };
                right_shape.push_back(p);
            }
            else if (cross_num == 2) {
                p.xyz = { s.point[i].xyz.x, s.point[i].xyz.y, 1.0 };
                left_shape.push_back(p);
            }
        }
    }
}

void make_particle(float sx, float sy, GLfloat sr, GLfloat sg, GLfloat sb) {
    temp_particle.point.clear(); temp_particle.color.clear();
    p.xyz = { 0.0f, 0.5f, 1.0f }; temp_particle.point.push_back(p);
    p.xyz = { -0.5f, -0.5f, 1.0f }; temp_particle.point.push_back(p);
    p.xyz = { 0.5f, -0.5f, 1.0f }; temp_particle.point.push_back(p);
    c.r = sr; c.g = sg; c.b = sb; 
    for (int i = 0; i < 3; ++i) temp_particle.color.push_back(c);
    temp_particle.x = sx; temp_particle.y = sy;

    TR = glm::mat4(1.0f);
    TR = glm::translate(TR, glm::vec3(sx, sy, 0.0f));
    TR = glm::scale(TR, glm::vec3(0.05f, 0.05f, 0.05f));

    for (int i = 0; i < temp_particle.point.size(); ++i) {
        transformedVertex = TR * glm::vec4(temp_particle.point[i].xyz, 1.0f);
        temp_particle.point[i].xyz = glm::vec3(transformedVertex);
    }

    for (int i = 0; i < 50; ++i) {
        temp_particle.rotate_r = uid_particle(dre);
        temp_particle.scale = uid(dre);
        temp_particle.tx = uid_particle(dre); 
        temp_particle.ty = uid_particle(dre);
        particle.push_back(temp_particle);
    }
}

void split_shape(Shape s) {
    // left
    temp_shape.point.clear(); temp_shape.color.clear();

    temp_shape.vertex_num = left_shape.size();

    c.r = s.color[0].r; c.g = s.color[0].g; c.b = s.color[0].b;

    for (int i = 0; i < temp_shape.vertex_num; ++i) {
        p.xyz = left_shape[i].xyz;
        temp_shape.point.push_back(p);
        temp_shape.color.push_back(c);
    }
    temp_shape.x = s.x; temp_shape.y = s.y;
    temp_shape.tx = -0.005f; temp_shape.ty = -0.01f;
    temp_shape.rotate_r = 0.03f;
    temp_shape.split = false; temp_shape.on_box = false; temp_shape.draw_route = false;
    shape.push_back(temp_shape);

    // right
    temp_shape.point.clear(); temp_shape.color.clear();
    temp_shape.vertex_num = right_shape.size();
    c.r = s.color[0].r; c.g = s.color[0].g; c.b = s.color[0].b;
    for (int i = 0; i < temp_shape.vertex_num; ++i) {
        p.xyz = right_shape[i].xyz;
        temp_shape.point.push_back(p);
        temp_shape.color.push_back(c);
    }
    temp_shape.x = s.x; temp_shape.y = s.y;
    temp_shape.tx = 0.005f; temp_shape.ty = -0.01f;
    temp_shape.rotate_r = 0.03f;
    temp_shape.split = false; temp_shape.on_box = false; temp_shape.draw_route = false;
    shape.push_back(temp_shape);

    make_particle(s.x, s.y, s.color[0].r, s.color[0].g, s.color[0].b);
}

bool collide_box(Shape s) {
    if (s.x >= box[0][0] && s.x <= box[3][0]) {
        for (int i = 0; i < s.point.size(); ++i) {
            if (s.point[i].xyz.y <= box[0][1])return true;
        }
    }
    return false;
}

char* filetobuf(const char* file)
{
    FILE* fptr;
    long length;
    char* buf;
    fptr = fopen(file, "rb");
    if (!fptr) 
        return NULL;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0; 
    return buf;
}

void make_newShape() {
    temp_shape.s = uidd(dre);
    temp_shape.vertex_num = 0;
    temp_shape.point.clear(); temp_shape.color.clear();
    if (temp_shape.s == 0) { // 삼각형
        p.xyz = { 0.0f, 0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { -0.5f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.5f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;

        c.r = uid(dre); c.g = uid(dre); c.b = uid(dre);
        for (int i = 0; i < 3; ++i) temp_shape.color.push_back(c);
    }
    else if (temp_shape.s == 1) { // 사각형
        p.xyz = { -0.5f, 0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { -0.5f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.5f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.5f, 0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        c.r = uid(dre); c.g = uid(dre); c.b = uid(dre);
        for (int i = 0; i < 4; ++i) temp_shape.color.push_back(c);
    }
    else if (temp_shape.s == 2) { // 오각형
        p.xyz = { 0.0f, 0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { -0.5f, 0.15f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { -0.3f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.3f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.5f, 0.15f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        c.r = uid(dre); c.g = uid(dre); c.b = uid(dre);
        for (int i = 0; i < 5; ++i) temp_shape.color.push_back(c);
    }
    else if (temp_shape.s == 3) { // 육각형
        p.xyz = { -0.25f, 0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { -0.55f, 0.0f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { -0.25f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.25f, -0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.55f, 0.0f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        p.xyz = { 0.25f, 0.5f, 1.0f }; temp_shape.point.push_back(p); temp_shape.vertex_num++;
        c.r = uid(dre); c.g = uid(dre); c.b = uid(dre);
        for (int i = 0; i < 56; ++i) temp_shape.color.push_back(c);
    }
    temp_shape.rotate_r = 0.03f; temp_shape.split = false; temp_shape.on_box = false; temp_shape.draw_route = true;
    int rand_x = uid_x(dre);
    if (rand_x == 0) {
        temp_shape.x = 1.2f; temp_shape.lean = -(uid_lean(dre));
        temp_shape.tx = -0.01f; temp_shape.ty = temp_shape.lean * temp_shape.tx;
    }
    else if (rand_x == 1) {
        temp_shape.x = -1.2f; temp_shape.lean = uid_lean(dre);
        temp_shape.tx = 0.01f; temp_shape.ty = temp_shape.lean * temp_shape.tx;
    }
    temp_shape.y = 0.0f;
    
    TR = glm::mat4(1.0f);
    TR = glm::translate(TR, glm::vec3(temp_shape.x, temp_shape.y, 0.0f));
    TR = glm::scale(TR, glm::vec3(0.3f, 0.3f, 0.3f));

    for (int i = 0; i < temp_shape.point.size(); ++i) {
        transformedVertex = TR * glm::vec4(temp_shape.point[i].xyz, 1.0f);
        temp_shape.point[i].xyz = glm::vec3(transformedVertex);
    }

    shape.push_back(temp_shape);
}

void make_shaderProgram()
{
    make_vertexShaders(); //--- 버텍스 세이더 만들기
    make_fragmentShaders(); //--- 프래그먼트 세이더 만들기

    //-- shader Program
    shaderProgramID = glCreateProgram();

    glAttachShader(shaderProgramID, vertexShader);
    glAttachShader(shaderProgramID, fragmentShader);
    glLinkProgram(shaderProgramID);

    //--- 세이더 삭제하기
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    //--- Shader Program 사용하기
    glUseProgram(shaderProgramID);
}

void make_vertexShaders()
{
    vertexSource = filetobuf("vertex.glsl");

    //--- 버텍스 세이더 객체 만들기
    vertexShader = glCreateShader(GL_VERTEX_SHADER);

    //--- 세이더 코드를 세이더 객체에 넣기
    glShaderSource(vertexShader, 1, (const GLchar**)&vertexSource, 0);

    //--- 버텍스 세이더 컴파일하기
    glCompileShader(vertexShader);

    //--- 컴파일이 제대로 되지 않은 경우: 에러 체크
    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
        std::cout << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
        return;
    }
}

void make_fragmentShaders()
{
    fragmentSource = filetobuf("fragment.glsl");

    //--- 프래그먼트 세이더 객체 만들기
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    //--- 세이더 코드를 세이더 객체에 넣기
    glShaderSource(fragmentShader, 1, (const GLchar**)&fragmentSource, 0);

    //--- 프래그먼트 세이더 컴파일
    glCompileShader(fragmentShader);

    //--- 컴파일이 제대로 되지 않은 경우: 컴파일 에러 체크
    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
        std::cout << "ERROR: fragment shader 컴파일 실패\n" << errorLog << std::endl;
        return;
    }
}

void InitBuffer()
{
    glGenVertexArrays(1, &VAO); //--- VAO 를 지정하고 할당하기
    glBindVertexArray(VAO); //--- VAO를 바인드하기
    glGenBuffers(2, VBO); //--- 3개의 VBO를 지정하고 할당하기
}

int new_shape_time = 0;
void TimerFunc(int value)
{
    // 새로운 도형 생성
    new_shape_time++;
    if (new_shape_time >= 100) {
        make_newShape();
        new_shape_time = 0;
    }

    // 도형 이동
    for (int i = 0; i < shape.size(); ++i) {
        if (shape[i].on_box)shape[i].tx = box_xmove;

        TR = glm::mat4(1.0f);
        TR = glm::translate(TR, glm::vec3(shape[i].x, shape[i].y, 0.0f));
        TR = glm::rotate(TR, shape[i].rotate_r, glm::vec3(0.0f, 0.0f, 1.0f));
        TR = glm::translate(TR, glm::vec3(-shape[i].x, -shape[i].y, 0.0f));

        TR = glm::translate(TR, glm::vec3(shape[i].tx, shape[i].ty, 0.0f));
        shape[i].x += shape[i].tx;
        shape[i].y += shape[i].ty;
        for (int j = 0; j < shape[i].point.size(); ++j) {
            transformedVertex = TR * glm::vec4(shape[i].point[j].xyz, 1.0f);
            shape[i].point[j].xyz = glm::vec3(transformedVertex);
        }
    }

    // 바구니 이동
    TR = glm::mat4(1.0f);
    TR = glm::translate(TR, glm::vec3(box_xmove, 0.0f, 0.0f));
    box_x += box_xmove;
    if (box_x >= 0.75)box_xmove = -box_xmove;
    else if (box_x <= -0.75)box_xmove = -box_xmove;
    for (int i = 0; i < 4; ++i) {
        transformedVertex = TR * glm::vec4(box[i][0], box[i][1], 1.0f, 1.0f);
        p.xyz = glm::vec3(transformedVertex);
        box[i][0] = p.xyz.x; box[i][1] = p.xyz.y;
    }

    // 바구니와 도형 충돌처리
    for (int i = 0; i < shape.size(); ++i) {
        if (collide_box(shape[i])) {
            shape[i].tx = box_xmove; shape[i].ty = 0.0f;
            shape[i].rotate_r = 0.0f;
        }
    }

    // 범위 넘어가거나 쪼개진 도형 삭제
    for (int i = 0; i < shape.size();) {
        if (shape[i].x <= -1.3f || shape[i].x >= 1.3f || shape[i].split) {
            shape.erase(shape.begin() + i);
        }
        else {
            ++i;
        }
    }

    // 파티클 이동 및 삭제
    for (int i = 0; i < particle.size(); ++i) {
        TR = glm::mat4(1.0f);
        TR = glm::translate(TR, glm::vec3(particle[i].x, particle[i].y, 0.0f));
        TR = glm::rotate(TR, particle[i].rotate_r, glm::vec3(0.0f, 0.0f, 1.0f));
        TR = glm::translate(TR, glm::vec3(-particle[i].x, -particle[i].y, 0.0f));

        TR = glm::translate(TR, glm::vec3(particle[i].tx, particle[i].ty, 0.0f));

        TR = glm::scale(TR, glm::vec3(particle[i].scale, particle[i].scale, particle[i].scale));
        particle[i].x += particle[i].tx;
        particle[i].y += particle[i].ty;
        for (int j = 0; j < particle[i].point.size(); ++j) {
            transformedVertex = TR * glm::vec4(particle[i].point[j].xyz, 1.0f);
            particle[i].point[j].xyz = glm::vec3(transformedVertex);
        }
    }

    glutPostRedisplay();
    glutTimerFunc(timer_speed, TimerFunc, 1);
}

GLvoid Keyboard(unsigned char key, int x, int y)
{
    switch (key) {
    case 'l': // line / fill mode
        shape_mode = !shape_mode;
        break;
    case 'r': // route on / off
        route_mode = !route_mode;
        break;
    case '+':
        if (timer_speed > 5) timer_speed--;
        break;
    case '-':
        timer_speed++;
        break;
    case 'q':
        glutDestroyWindow(1);
        break;
    }
    glutPostRedisplay();
}

double mx, my;
int shape_size = 0;
void Mouse(int button, int state, int x, int y)
{
    mx = ((double)x - WINDOWX / 2.0) / ( WINDOWX / 2.0);
    my = -(((double)y - WINDOWY / 2.0) / (WINDOWY / 2.0));

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        mouse_click = true;
        line[0][0] = mx; line[0][1] = my; line[0][2] = 1.0f;
        line[1][0] = mx; line[1][1] = my; line[1][2] = 1.0f;
    }
    else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        mouse_click = false;
        shape_size = shape.size();
        for (int i = 0; i < shape_size; ++i) {
            if (!shape[i].split) {
                Collide_shape(shape[i]);
                if (cross_num == 2) {
                    split_shape(shape[i]);
                    shape[i].split = true;
                }
            }
        }
    }
}
void Motion(int x, int y)
{
    mx = ((double)x - WINDOWX / 2.0) / (WINDOWX / 2.0);
    my = -(((double)y - WINDOWY / 2.0) / (WINDOWY / 2.0));
    if (mouse_click) {
        line[1][0] = mx; line[1][1] = my; line[1][2] = 1.0f;
    }
}


GLvoid drawScene() //--- 콜백 함수: 그리기 콜백 함수
{
    glClearColor(0.69f, 0.83f, 0.94f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);    //배경

    glUseProgram(shaderProgramID);
    glBindVertexArray(VAO);// 쉐이더 , 버퍼 배열 사용

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 도형, 도형 경로 그리기
    for (int i = 0; i < shape.size(); ++i) {
        if (route_mode && shape[i].draw_route && !shape[i].split) {
            route_line[0][0] = shape[i].x; route_line[0][1] = shape[i].y;
            if (shape[i].tx >= 0.0f) {
                route_line[1][0] = shape[i].x + 2.0f;
                route_line[1][1] = shape[i].y + 2.0f * shape[i].lean;
            }
            else {
                route_line[1][0] = shape[i].x - 2.0f;
                route_line[1][1] = shape[i].y - 2.0f * shape[i].lean;
            }
            glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), route_line, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
            glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), line_color, GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(1);

            glBindVertexArray(VAO);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, 2);
        }

        if (!shape[i].split) {
            shape[i].bind();
            shape[i].draw();
        }
    }
    // 선 그리기
    if (mouse_click) {
        glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), line, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), line_color, GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);

        glBindVertexArray(VAO);
        glLineWidth(1.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 2);
    }

    // 파티클 그리기
    for (int i = 0; i < particle.size(); ++i) {
        particle[i].bind();
        particle[i].draw();
    }

    // 바구니 그리기
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), box, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), box_color, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(VAO);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glutSwapBuffers(); // 화면에 출력하기
}

GLvoid Reshape(int w, int h) //--- 콜백 함수: 다시 그리기 콜백 함수
{
    glViewport(0, 0, w, h);
}

//--- 메인 함수
void main(int argc, char** argv) //--- 윈도우 출력하고 콜백함수 설정
{
    //--- 윈도우 생성하기
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(WINDOWX, WINDOWY);
    glutCreateWindow("Let's SP!");

    make_newShape();

    //--- GLEW 초기화하기
    glewExperimental = GL_TRUE;
    glewInit();
    make_shaderProgram();
    InitBuffer();
    glutTimerFunc(timer_speed, TimerFunc, 1);
    glutKeyboardFunc(Keyboard);
    glutMouseFunc(Mouse);
    glutMotionFunc(Motion);
    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);

    glutMainLoop();
}