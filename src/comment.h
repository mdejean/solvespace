#ifndef __COMMENT_H
#define __COMMENT_H

#include "command.h"

class Comment : public CommandBase {
    public:
    virtual void Menu(Command id);
    virtual void MouseMoved(const Point2d& v, bool leftDown, bool middleDown,
        bool rightDown, bool shiftDown, bool ctrlDown);
    virtual void MouseLeftDown(const Vector& v); //FIXME: point2d or vector? be consistent
    virtual void MouseRightUp(const Point2d& v);
    virtual const char* Description() const;
};

#endif
