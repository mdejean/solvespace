#ifndef __COMMAND_REQUEST_H
#define __COMMAND_REQUEST_H

#include "command.h"

class CommandRequest : public CommandBase {
    public:
    const char* description;
    virtual void Menu(Command id);
    virtual void MouseMoved(const Point2d& v, bool leftDown, bool middleDown, 
        bool rightDown, bool shiftDown, bool ctrlDown);
    virtual void MouseLeftDown(const Vector& v); //FIXME: point2d or vector? be consistent
    virtual void MouseRightUp(const Point2d& v);
    virtual const char* Description() const;
};

#endif