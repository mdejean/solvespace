#include "solvespace.h"

void Comment::Menu(Command id) {
    SS.GW.pending.operation = Pending::COMMAND;
    SS.GW.pending.command = Command::COMMENT;
    SS.ScheduleShowTW();
}

void Comment::MouseMoved(const Point2d& mp, bool leftDown, bool middleDown,
    bool rightDown, bool shiftDown, bool ctrlDown) {

}

void Comment::MouseLeftDown(const Vector& v) {
    Constraint c = {};
    c.group       = SS.GW.activeGroup;
    c.workplane   = SS.GW.ActiveWorkplane();
    c.type        = Constraint::Type::COMMENT;
    c.disp.offset = v;
    c.comment     = _("NEW COMMENT -- DOUBLE-CLICK TO EDIT");
    hConstraint hc = Constraint::AddConstraint(&c);
    Group* g = nullptr;
    if(hc.v != 0) {
        g = SK.GetGroup(SK.GetConstraint(hc)->group);
    }
    if(g != nullptr) {
        g->visible = true;
    }
    SS.GW.ClearSuper();
}

void Comment::MouseRightUp(const Point2d& mp) {
}

const char* Comment::Description() const {
    return _("click center of comment text");
}
