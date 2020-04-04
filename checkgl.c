/*

 *  make : gcc checkgl.c -lglut -lGL -lGLU -o checkgl

 *  usage: ./checkgl

 *  result sample:

(1)
 GL_VENDOR   : X.Org
 GL_RENDERER : Gallium 0.4 on AMD CAICOS (DRM 2.49.0 / 4.4.131-20190905.kylin.desktop-generic
 GL_VERSION  : 3.0 Mesa 11.2.0
GLU_VERSION  : 1.3

(2)
 GL_VENDOR   : X.Org
 GL_RENDERER : Gallium 0.4 on AMD CAICOS (DRM 2.48.0 / 4.4.58-20170818.kylin.5.desktop-generi
 GL_VERSION  : 3.0 Mesa 11.2.0
GLU_VERSION  : 1.3

(3)
 GL_VENDOR   : X.Org
 GL_RENDERER : Gallium 0.4 on AMD CAICOS (DRM 2.49.0 / 4.4.58-20190412.kylin.desktop-generic,
 GL_VERSION  : 3.0 Mesa 11.2.0
GLU_VERSION  : 1.3

 *  by ZZX@Beijing (C)CESI 2020-02-28, refweb

 */

#include <stdio.h>
#include <GL/glut.h>

int main(int argc, char** argv)
{
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_SINGLE|GLUT_RGB|GLUT_DEPTH);
	glutInitWindowSize(300,300);
	glutInitWindowPosition(100,100);
	glutCreateWindow("OpenGL Version");

	const GLubyte* name        = glGetString ( GL_VENDOR  ); // 返回负责当前OpenGL实现厂商的名字
	const GLubyte* renderer    = glGetString ( GL_RENDERER); // 返回一个渲染器标识符，通常是个硬件平台
	const GLubyte* version     = glGetString ( GL_VERSION ); // 返回当前OpenGL实现的版本号
	const GLubyte* glu_version = gluGetString(GLU_VERSION ); // 返回当前GLU工具库版本

	printf(" GL_VENDOR   : %s\n", name        );
	printf(" GL_RENDERER : %s\n", renderer    );
	printf(" GL_VERSION  : %s\n", version     );
	printf("GLU_VERSION  : %s\n", glu_version );

	return 0;
}
