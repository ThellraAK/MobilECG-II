#include "Curve.h"
#include "EcgArea.h"
#include "Helper.h"
#include "ShaderBuilder.h"
#include <algorithm>
#include "log.h"

int Curve::xCoordinatesLength=0;
GLfloat *Curve::xCoordinates=NULL;
GLuint Curve::xCoordinatesOnGPU=0;
const float Curve::POINT_INVALID=0.0/0.0;

std::vector<GLfloat> Curve::invalidBuffer(1000, Curve::POINT_INVALID);

GLuint Curve::getXCoordinates(int capacity){
    capacity = std::max(capacity, 10000);
    if (capacity>xCoordinatesLength || EcgArea::instance().isRedrawNeeded()){
        if (xCoordinatesLength==0){
            glGenBuffers(1, &xCoordinatesOnGPU);
        } else {
            delete [] xCoordinates;
        }

        xCoordinates = new GLfloat[capacity];
        xCoordinatesLength = capacity;

        for (int a=0; a<capacity; a++){
            xCoordinates[a]=a;
        }

        glBindBuffer(GL_ARRAY_BUFFER , xCoordinatesOnGPU);
        glBufferData(GL_ARRAY_BUFFER , sizeof(GLfloat)*capacity, xCoordinates, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER , 0);
    }

    return xCoordinatesOnGPU;
}

Curve::Curve(): DrawableObject(){
    scale.x=1;
    scale.y=3;
    position.y=100;
    currNumOfPoints=0;
    requiredNumOfPoints=1;
    clearWidthInPoints=100;

    color[0]=1;
    color[1]=0;
    color[2]=0;
}

void Curve::init(AAssetManager *assetManager){
    vertexShader = helper::loadAsset(assetManager, "curve.vert");
    fragmentShader = helper::loadAsset(assetManager, "curve.frag");
}

void Curve::clear(){
    //Fill value with invalid data
    glBindBuffer(GL_ARRAY_BUFFER , valueBuffer);
    for (int offset=0; offset<currNumOfPoints; offset+=invalidBuffer.size()){
        int transferblock = std::min((int)invalidBuffer.size(), currNumOfPoints - offset);
        glBufferSubData(GL_ARRAY_BUFFER, offset*sizeof(GLfloat), transferblock*sizeof(GLfloat), invalidBuffer.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER , 0);
}

void Curve::setLength(int pLengthInPixels){
    lengthInPixels = pLengthInPixels;
    requiredNumOfPoints = lengthInPixels / scale.x;
}

void Curve::glInit(){
    shaderId = ShaderBuilder::instance().buildShader("Curve", vertexShader, fragmentShader);
    GLuint shaderProgram = ShaderBuilder::instance().getShader(shaderId);

    shader_a_Position=helper::getGlAttributeWithAssert(shaderProgram, "a_Position");
    shader_screenSize=helper::getGlUniformWithAssert(shaderProgram, "screenSize");
    shader_position=helper::getGlUniformWithAssert(shaderProgram, "position");
    shader_scale=helper::getGlUniformWithAssert(shaderProgram, "scale");
    shader_color=helper::getGlUniformWithAssert(shaderProgram, "color");
    shader_a_Value=helper::getGlAttributeWithAssert(shaderProgram, "a_Value");

    shader_endOffset=helper::getGlUniformWithAssert(shaderProgram, "endOffset");
    shader_clearWidth=helper::getGlUniformWithAssert(shaderProgram, "clearWidth");
    shader_pointCount=helper::getGlUniformWithAssert(shaderProgram, "pointCount");

    glGenBuffers(1, &valueBuffer);

    getXCoordinates(currNumOfPoints);
}

void Curve::put(GLfloat *data, int n){
    newPointBuffer.add(data, n);
}

void Curve::resizeOnGPU(){
    if (requiredNumOfPoints==currNumOfPoints && (!EcgArea::instance().isRedrawNeeded())){
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER , valueBuffer);
    glBufferData(GL_ARRAY_BUFFER , sizeof(GLfloat)*requiredNumOfPoints, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER , 0);

    currNumOfPoints = requiredNumOfPoints;
    currWritePos=0;
    clear();
}

void Curve::moveNewDataToGPU(){
    int remaining=newPointBuffer.used();

    if (!remaining)
        return;

    glBindBuffer(GL_ARRAY_BUFFER , valueBuffer);
    while (remaining){
        //LOGD("Writing %d to buffer", remaining);
        GLfloat *buffer;

        int transferSize=std::min(newPointBuffer.getContinuousReadBuffer(buffer), remaining);
        transferSize=std::min(transferSize, (int)(currNumOfPoints-currWritePos));

        glBufferSubData(GL_ARRAY_BUFFER, currWritePos*sizeof(GLfloat), transferSize*sizeof(GLfloat), buffer);

        remaining -= transferSize;
        currWritePos += transferSize;
        newPointBuffer.skip(transferSize);

        if (currWritePos >= currNumOfPoints){
            currWritePos = 0;
        }
    }
}

void Curve::draw(){
    resizeOnGPU();
    moveNewDataToGPU();

    GLuint shaderProgram = ShaderBuilder::instance().useProgram(shaderId);

    glBindBuffer(GL_ARRAY_BUFFER, getXCoordinates(currNumOfPoints));
    glVertexAttribPointer(shader_a_Position, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader_a_Position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUniform2f(shader_screenSize, screenSize[0],screenSize[1]);
    glUniform3f(shader_position, position[0], position[1], zCoordinate);
    glUniform2f(shader_scale, scale[0], scale[1]);
    glUniform3f(shader_color, color[0], color[1], color[2]);

    glUniform1f(shader_endOffset, currWritePos);
    glUniform1f(shader_pointCount, currNumOfPoints);
    glUniform1f(shader_clearWidth, clearWidthInPoints);

    glBindBuffer(GL_ARRAY_BUFFER, valueBuffer);
    glVertexAttribPointer(shader_a_Value, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader_a_Value);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glLineWidth(3);
    glDrawArrays(GL_LINE_STRIP, 0, currNumOfPoints);
    glLineWidth(1);
}

void Curve::contextResized(int w, int h){
    screenSize.w = w;
    screenSize.h = h;
}

void Curve::setPosition(int x, int y) {
    position.x=x;
    position.y=y;
}

void Curve::setScale(float x, float y){
    scale.x=x;
    scale.y=y;

    setLength(lengthInPixels);
}
