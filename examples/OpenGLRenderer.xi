// Requires GLFW and OpenGL development headers/libraries; uses the OpenGL 2.1 fixed-function API.
usingC <GLFW/glfw3.h>;
usingC <GL/gl.h>;
usingL <glfw>;
usingL <GL>;

type GLenum = uint;
type GLbitfield = uint;
type GLfloat = float;
type GLFWwindow = external GLFWwindow*;

extern glfw {
    int glfwInit();
    void glfwWindowHint(int hint, int value);
    GLFWwindow glfwCreateWindow(int width, int height, string title, unsafe void* monitor, unsafe void* share);
    void glfwMakeContextCurrent(GLFWwindow window);
    void glfwSwapInterval(int interval);
    int glfwWindowShouldClose(GLFWwindow window);
    void glfwSwapBuffers(GLFWwindow window);
    void glfwPollEvents();
    void glfwDestroyWindow(GLFWwindow window);
    void glfwTerminate();
}

extern gl {
    void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void glClear(GLbitfield mask);
    void glBegin(GLenum mode);
    void glColor3f(GLfloat red, GLfloat green, GLfloat blue);
    void glVertex2f(GLfloat x, GLfloat y);
    void glEnd();
}

int main()
{
    if (glfw.glfwInit() == 0) {
        Console.WriteLine("GLFW initialization failed");
        return FAILURE;
    }

    glfw.glfwWindowHint(139266, 2);
    glfw.glfwWindowHint(139267, 1);
    GLFWwindow window = glfw.glfwCreateWindow(640, 480, "XiHash OpenGL renderer", null, null);
    if (window == null) {
        glfw.glfwTerminate();
        Console.WriteLine("Could not create an OpenGL window");
        return FAILURE;
    }

    glfw.glfwMakeContextCurrent(window);
    glfw.glfwSwapInterval(1);

    while (glfw.glfwWindowShouldClose(window) == 0) {
        gl.glClearColor(0.08, 0.10, 0.16, 1.0);
        gl.glClear(16384);
        gl.glBegin(4);
        gl.glColor3f(1.0, 0.2, 0.15);
        gl.glVertex2f(0.0, 0.75);
        gl.glColor3f(0.2, 0.9, 0.35);
        gl.glVertex2f(-0.7, -0.55);
        gl.glColor3f(0.2, 0.45, 1.0);
        gl.glVertex2f(0.7, -0.55);
        gl.glEnd();

        glfw.glfwSwapBuffers(window);
        glfw.glfwPollEvents();
    }

    glfw.glfwDestroyWindow(window);
    glfw.glfwTerminate();
    return SUCCESS;
}
