#include "solvespace.h"

void CommandRequest::Menu(Command id) {
    const char *s;
    switch (id) {
        case Command::ARC: s = _("click point on arc (draws anti-clockwise)"); goto c;
        case Command::DATUM_POINT: s = _("click to place datum point"); goto c;
        case Command::LINE_SEGMENT: s = _("click first point of line segment"); goto c;
        case Command::CONSTR_SEGMENT:
            s = _("click first point of construction line segment"); goto c;
        case Command::CUBIC: s = _("click first point of cubic segment"); goto c;
        case Command::CIRCLE: s = _("click center of circle"); goto c;
        case Command::WORKPLANE: s = _("click origin of workplane"); goto c;
        case Command::RECTANGLE: s = _("click one corner of rectangle"); goto c;
        case Command::TTF_TEXT: s = _("click top left of text"); goto c;
c:
            SS.GW.pending.operation = Pending::COMMAND;
            SS.GW.pending.command = id;
            SS.GW.pending.command_c = this;
            SS.GW.pending.description = s;
            SS.ScheduleShowTW();
            InvalidateGraphics(); // repaint toolbar
            break;
        default: ssassert(false, "Unexpected pending menu id");
    }
}

void CommandRequest::MouseMoved(const Point2d& mp, bool leftDown, bool middleDown, 
    bool rightDown, bool shiftDown, bool ctrlDown)
{
    //TODO: see if these can be made members
    auto& pending = SS.GW.pending;
    auto& orig = SS.GW.orig;
    auto x = mp.x, y = mp.y;
    
    switch(pending.operation) {
        case Pending::DRAGGING_NEW_LINE_POINT:
            if(!ctrlDown) {
                SS.GW.pending.hasSuggestion =
                    SS.GW.SuggestLineConstraint(SS.GW.pending.request, &SS.GW.pending.suggestion);
            } else {
                SS.GW.pending.hasSuggestion = false;
            }
        case Pending::DRAGGING_NEW_POINT:
            SS.GW.UpdateDraggedPoint(pending.point, x, y);
            SS.GW.HitTestMakeSelection(mp);
            SS.MarkGroupDirtyByEntity(pending.point);
            orig.mouse = mp;
            InvalidateGraphics();
            break;
        case Pending::DRAGGING_NEW_CUBIC_POINT: {
            SS.GW.UpdateDraggedPoint(pending.point, x, y);
            SS.GW.HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            if(pending.point.v == hr.entity(4).v) {
                // The very first segment; dragging final point drags both
                // tangent points.
                Vector p0 = SK.GetEntity(hr.entity(1))->PointGetNum(),
                       p3 = SK.GetEntity(hr.entity(4))->PointGetNum(),
                       p1 = p0.ScaledBy(2.0/3).Plus(p3.ScaledBy(1.0/3)),
                       p2 = p0.ScaledBy(1.0/3).Plus(p3.ScaledBy(2.0/3));
                SK.GetEntity(hr.entity(1+1))->PointForceTo(p1);
                SK.GetEntity(hr.entity(1+2))->PointForceTo(p2);
            } else {
                // A subsequent segment; dragging point drags only final
                // tangent point.
                int i = SK.GetEntity(hr.entity(0))->extraPoints;
                Vector pn   = SK.GetEntity(hr.entity(4+i))->PointGetNum(),
                       pnm2 = SK.GetEntity(hr.entity(2+i))->PointGetNum(),
                       pnm1 = (pn.Plus(pnm2)).ScaledBy(0.5);
                SK.GetEntity(hr.entity(3+i))->PointForceTo(pnm1);
            }

            orig.mouse = mp;
            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case Pending::DRAGGING_NEW_ARC_POINT: {
            SS.GW.UpdateDraggedPoint(pending.point, x, y);
            SS.GW.HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            Vector ona = SK.GetEntity(hr.entity(2))->PointGetNum();
            Vector onb = SK.GetEntity(hr.entity(3))->PointGetNum();
            Vector center = (ona.Plus(onb)).ScaledBy(0.5);

            SK.GetEntity(hr.entity(1))->PointForceTo(center);

            orig.mouse = mp;
            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        //FIXME: code duplicated from GraphicsWindow::MouseMoved
        case Pending::DRAGGING_NEW_RADIUS: {
            Entity *circle = SK.GetEntity(pending.circle);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            Point2d c2 = SS.GW.ProjectPoint(center);
            double r = c2.DistanceTo(mp)/SS.GW.scale;
            SK.GetEntity(circle->distance)->DistanceForceTo(r);

            SS.MarkGroupDirtyByEntity(pending.circle);
            break;
        }
        default: {
        }
    }
}

void CommandRequest::MouseRightUp(const Point2d& mp) {
    auto& pending = SS.GW.pending;
    if(pending.operation == Pending::DRAGGING_NEW_LINE_POINT && pending.hasSuggestion) {
        Constraint::Constrain(SS.GW.pending.suggestion,
            Entity::NO_ENTITY, Entity::NO_ENTITY, pending.request.entity(0));
    }

    if(pending.operation == Pending::DRAGGING_NEW_LINE_POINT ||
       pending.operation == Pending::DRAGGING_NEW_CUBIC_POINT)
    {
        // Special case; use a right click to stop drawing lines, since
        // a left click would draw another one. This is quicker and more
        // intuitive than hitting escape. Likewise for new cubic segments.
        SS.GW.ClearPending();
    }
}

void CommandRequest::MouseLeftDown(const Vector& v) {
    auto& pending = SS.GW.pending;
    auto& scale = SS.GW.scale;
    auto& hover = SS.GW.hover;
    
    hRequest hr = {};
    hConstraint hc = {};
    switch(pending.operation) {
        case Pending::COMMAND:
            switch(pending.command) {
                case Command::DATUM_POINT:
                    hr = SS.GW.AddRequest(Request::Type::DATUM_POINT);
                    SK.GetEntity(hr.entity(0))->PointForceTo(v);
                    SS.GW.ConstrainPointByHovered(hr.entity(0));

                    SS.GW.ClearSuper();
                    break;

                case Command::LINE_SEGMENT:
                case Command::CONSTR_SEGMENT:
                    hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT);
                    SK.GetRequest(hr)->construction = (pending.command == Command::CONSTR_SEGMENT);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    SS.GW.ConstrainPointByHovered(hr.entity(1));

                    SS.GW.AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_LINE_POINT;
                    pending.request = hr;
                    pending.point = hr.entity(2);
                    pending.description = _("click next point of line, or press Esc");
                    SK.GetEntity(pending.point)->PointForceTo(v);
                    break;

                case Command::RECTANGLE: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw rectangle in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        SS.GW.ClearSuper();
                        break;
                    }
                    hRequest lns[4];
                    int i;
                    SS.UndoRemember();
                    for(i = 0; i < 4; i++) {
                        lns[i] = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
                        SS.GW.AddToPending(lns[i]);
                    }
                    for(i = 0; i < 4; i++) {
                        Constraint::ConstrainCoincident(
                            lns[i].entity(1), lns[(i+1)%4].entity(2));
                        SK.GetEntity(lns[i].entity(1))->PointForceTo(v);
                        SK.GetEntity(lns[i].entity(2))->PointForceTo(v);
                    }
                    for(i = 0; i < 4; i++) {
                        Constraint::Constrain(
                            (i % 2) ? Constraint::Type::HORIZONTAL : Constraint::Type::VERTICAL,
                            Entity::NO_ENTITY, Entity::NO_ENTITY,
                            lns[i].entity(0));
                    }
                    if(SS.GW.ConstrainPointByHovered(lns[2].entity(1))) {
                        Vector pos = SK.GetEntity(lns[2].entity(1))->PointGetNum();
                        for(i = 0; i < 4; i++) {
                            SK.GetEntity(lns[i].entity(1))->PointForceTo(pos);
                            SK.GetEntity(lns[i].entity(2))->PointForceTo(pos);
                        }
                    }

                    pending.operation = Pending::DRAGGING_NEW_POINT;
                    pending.point = lns[1].entity(2);
                    pending.description = _("click to place other corner of rectangle");
                    hr = lns[0];
                    break;
                }
                case Command::CIRCLE:
                    hr = SS.GW.AddRequest(Request::Type::CIRCLE);
                    // Centered where we clicked
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    // Normal to the screen
                    SK.GetEntity(hr.entity(32))->NormalForceTo(
                        Quaternion::From(SS.GW.projRight, SS.GW.projUp));
                    // Initial radius zero
                    SK.GetEntity(hr.entity(64))->DistanceForceTo(0);

                    SS.GW.ConstrainPointByHovered(hr.entity(1));

                    pending.operation = Pending::DRAGGING_NEW_RADIUS;
                    pending.circle = hr.entity(0);
                    pending.description = _("click to set radius");
                    break;

                case Command::ARC: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw arc in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        SS.GW.ClearPending();
                        break;
                    }
                    hr = SS.GW.AddRequest(Request::Type::ARC_OF_CIRCLE);
                    // This fudge factor stops us from immediately failing to solve
                    // because of the arc's implicit (equal radius) tangent.
                    Vector adj = SS.GW.projRight.WithMagnitude(2/SS.GW.scale);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v.Minus(adj));
                    SK.GetEntity(hr.entity(2))->PointForceTo(v);
                    SK.GetEntity(hr.entity(3))->PointForceTo(v);
                    SS.GW.ConstrainPointByHovered(hr.entity(2));

                    SS.GW.AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_ARC_POINT;
                    pending.point = hr.entity(3);
                    pending.description = _("click to place point");
                    break;
                }
                case Command::CUBIC:
                    hr = SS.GW.AddRequest(Request::Type::CUBIC);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    SK.GetEntity(hr.entity(2))->PointForceTo(v);
                    SK.GetEntity(hr.entity(3))->PointForceTo(v);
                    SK.GetEntity(hr.entity(4))->PointForceTo(v);
                    SS.GW.ConstrainPointByHovered(hr.entity(1));

                    SS.GW.AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_CUBIC_POINT;
                    pending.point = hr.entity(4);
                    pending.description = _("click next point of cubic, or press Esc");
                    break;

                case Command::WORKPLANE:
                    if(SS.GW.LockedInWorkplane()) {
                        Error(_("Sketching in a workplane already; sketch in 3d before "
                                "creating new workplane."));
                        SS.GW.ClearSuper();
                        break;
                    }
                    hr = SS.GW.AddRequest(Request::Type::WORKPLANE);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    SK.GetEntity(hr.entity(32))->NormalForceTo(
                        Quaternion::From(SS.GW.projRight, SS.GW.projUp));
                    SS.GW.ConstrainPointByHovered(hr.entity(1));

                    SS.GW.ClearSuper();
                    break;

                case Command::TTF_TEXT: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw text in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        SS.GW.ClearSuper();
                        break;
                    }
                    hr = SS.GW.AddRequest(Request::Type::TTF_TEXT);
                    SS.GW.AddToPending(hr);
                    Request *r = SK.GetRequest(hr);
                    r->str = "Abc";
                    r->font = "arial.ttf";

                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    SK.GetEntity(hr.entity(2))->PointForceTo(v);

                    pending.operation = Pending::DRAGGING_NEW_POINT;
                    pending.point = hr.entity(2);
                    pending.description = _("click to place bottom left of text");
                    break;
                }

                case Command::COMMENT: {
                    SS.GW.ClearSuper();
                    Constraint c = {};
                    c.group       = SS.GW.activeGroup;
                    c.workplane   = SS.GW.ActiveWorkplane();
                    c.type        = Constraint::Type::COMMENT;
                    c.disp.offset = v;
                    c.comment     = _("NEW COMMENT -- DOUBLE-CLICK TO EDIT");
                    hc = Constraint::AddConstraint(&c);
                    break;
                }
                default: ssassert(false, "Unexpected pending menu id");
            }
            break;

        case Pending::DRAGGING_NEW_RADIUS:
            SS.GW.ClearPending();
            break;

        case Pending::DRAGGING_NEW_POINT:
        case Pending::DRAGGING_NEW_ARC_POINT:
            SS.GW.ConstrainPointByHovered(pending.point);
            SS.GW.ClearPending();
            break;

        case Pending::DRAGGING_NEW_CUBIC_POINT: {
            hRequest hr = pending.point.request();
            Request *r = SK.GetRequest(hr);

            if(hover.entity.v == hr.entity(1).v && r->extraPoints >= 2) {
                // They want the endpoints coincident, which means a periodic
                // spline instead.
                r->type = Request::Type::CUBIC_PERIODIC;
                // Remove the off-curve control points, which are no longer
                // needed here; so move [2,ep+1] down, skipping first pt.
                int i;
                for(i = 2; i <= r->extraPoints+1; i++) {
                    SK.GetEntity(hr.entity((i-1)+1))->PointForceTo(
                        SK.GetEntity(hr.entity(i+1))->PointGetNum());
                }
                // and move ep+3 down by two, skipping both
                SK.GetEntity(hr.entity((r->extraPoints+1)+1))->PointForceTo(
                  SK.GetEntity(hr.entity((r->extraPoints+3)+1))->PointGetNum());
                r->extraPoints -= 2;
                // And we're done.
                SS.MarkGroupDirty(r->group);
                SS.GW.ClearPending();
                break;
            }

            if(SS.GW.ConstrainPointByHovered(pending.point)) {
                SS.GW.ClearPending();
                break;
            }

            Entity e;
            if(r->extraPoints >= (int)arraylen(e.point) - 4) {
                SS.GW.ClearPending();
                break;
            }

            (SK.GetRequest(hr)->extraPoints)++;
            SS.GenerateAll(SolveSpaceUI::Generate::REGEN);

            int ep = r->extraPoints;
            Vector last = SK.GetEntity(hr.entity(3+ep))->PointGetNum();

            SK.GetEntity(hr.entity(2+ep))->PointForceTo(last);
            SK.GetEntity(hr.entity(3+ep))->PointForceTo(v);
            SK.GetEntity(hr.entity(4+ep))->PointForceTo(v);
            pending.point = hr.entity(4+ep);
            break;
        }

        case Pending::DRAGGING_NEW_LINE_POINT: {
            // Constrain the line segment horizontal or vertical if close enough
            if(pending.hasSuggestion) {
                Constraint::Constrain(SS.GW.pending.suggestion,
                    Entity::NO_ENTITY, Entity::NO_ENTITY, pending.request.entity(0));
            }

            if(hover.entity.v) {
                Entity *e = SK.GetEntity(hover.entity);
                if(e->IsPoint()) {
                    hRequest hrl = pending.point.request();
                    Entity *sp = SK.GetEntity(hrl.entity(1));
                    if(( e->PointGetNum()).Equals(
                       (sp->PointGetNum())))
                    {
                        // If we constrained by the hovered point, then we
                        // would create a zero-length line segment. That's
                        // not good, so just stop drawing.
                        SS.GW.ClearPending();
                        break;
                    }
                }
            }

            if(SS.GW.ConstrainPointByHovered(pending.point)) {
                SS.GW.ClearPending();
                break;
            }

            // Create a new line segment, so that we continue drawing.
            hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT);
            SS.GW.ReplacePending(pending.request, hr);
            SK.GetRequest(hr)->construction = SK.GetRequest(pending.request)->construction;
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            // Displace the second point of the new line segment slightly,
            // to avoid creating zero-length edge warnings.
            SK.GetEntity(hr.entity(2))->PointForceTo(
                v.Plus(SS.GW.projRight.ScaledBy(0.5/scale)));

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pending.point, hr.entity(1));

            // And drag an endpoint of the new line segment
            pending.operation = Pending::DRAGGING_NEW_LINE_POINT;
            pending.request = hr;
            pending.point = hr.entity(2);
            pending.description = _("click next point of line, or press Esc");

            break;
        }

        default:
            break;
    }

    // Activate group with newly created request/constraint
    Group *g = NULL;
    if(hr.v != 0) {
        g = SK.GetGroup(SK.GetRequest(hr)->group);
    }
    if(hc.v != 0) {
        g = SK.GetGroup(SK.GetConstraint(hc)->group);
    }
    if(g != NULL) {
        g->visible = true;
    }
}