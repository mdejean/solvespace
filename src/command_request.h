#ifndef __COMMAND_REQUEST_H
#define __COMMAND_REQUEST_H

//FIXME: other file
class CommandBase {
    public:
    virtual void Menu(Command id) = 0;
    virtual void MouseMoved(const Point2d& v, bool leftDown, bool middleDown, 
        bool rightDown, bool shiftDown, bool ctrlDown) = 0;
    virtual void MouseLeftDown(const Vector& v) = 0;
    virtual void MouseRightUp(const Point2d& v) = 0;
    //commands currently don't use key, middle, leftup, doubleclick rightdown.
    //consider adding them or making this more general
};

class CommandRequest : public CommandBase {
    public:
    virtual void Menu(Command id);
    virtual void MouseMoved(const Point2d& v, bool leftDown, bool middleDown, 
        bool rightDown, bool shiftDown, bool ctrlDown);
    virtual void MouseLeftDown(const Vector& v); //FIXME: point2d or vector? be consistent
    virtual void MouseRightUp(const Point2d& v);
};

#endif