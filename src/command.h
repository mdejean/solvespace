#ifndef __COMMAND_H
#define __COMMAND_H

//fixme: this should be named command

class CommandBase {
    public:
    virtual void Menu(Command id) = 0;
    virtual void MouseMoved(const Point2d& v, bool leftDown, bool middleDown,
        bool rightDown, bool shiftDown, bool ctrlDown) = 0;
    virtual void MouseLeftDown(const Vector& v) = 0;
    virtual void MouseRightUp(const Point2d& v) = 0;
    //commands currently don't use key, middle, leftup, doubleclick rightdown.
    //consider adding them or making this more general
    virtual const char* Description() const = 0;
};

#endif