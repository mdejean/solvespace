//-----------------------------------------------------------------------------
// Anything relating to mouse, keyboard, or 6-DOF mouse input.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void GraphicsWindow::UpdateDraggedPoint(hEntity hp, double mx, double my) {
    Entity *p = SK.GetEntity(hp);
    Vector pos = p->PointGetNum();
    UpdateDraggedNum(&pos, mx, my);
    p->PointForceTo(pos);
}

void GraphicsWindow::UpdateDraggedNum(Vector *pos, double mx, double my) {
    *pos = pos->Plus(projRight.ScaledBy((mx - orig.mouse.x)/scale));
    *pos = pos->Plus(projUp.ScaledBy((my - orig.mouse.y)/scale));
}

void GraphicsWindow::AddPointToDraggedList(hEntity hp) {
    Entity *p = SK.GetEntity(hp);
    // If an entity and its points are both selected, then its points could
    // end up in the list twice. This would be bad, because it would move
    // twice as far as the mouse pointer...
    List<hEntity> *lhe = &(pending.points);
    for(hEntity *hee = lhe->First(); hee; hee = lhe->NextAfter(hee)) {
        if(hee->v == hp.v) {
            // Exact same point.
            return;
        }
        Entity *pe = SK.GetEntity(*hee);
        if(pe->type == p->type &&
           pe->type != Entity::Type::POINT_IN_2D &&
           pe->type != Entity::Type::POINT_IN_3D &&
           pe->group.v == p->group.v)
        {
            // Transform-type point, from the same group. So it handles the
            // same unknowns.
            return;
        }
    }
    pending.points.Add(&hp);
}

void GraphicsWindow::StartDraggingByEntity(hEntity he) {
    Entity *e = SK.GetEntity(he);
    if(e->IsPoint()) {
        AddPointToDraggedList(e->h);
    } else if(e->type == Entity::Type::LINE_SEGMENT ||
              e->type == Entity::Type::ARC_OF_CIRCLE ||
              e->type == Entity::Type::CUBIC ||
              e->type == Entity::Type::CUBIC_PERIODIC ||
              e->type == Entity::Type::CIRCLE ||
              e->type == Entity::Type::TTF_TEXT)
    {
        int pts;
        EntReqTable::GetEntityInfo(e->type, e->extraPoints,
            NULL, &pts, NULL, NULL);
        for(int i = 0; i < pts; i++) {
            AddPointToDraggedList(e->point[i]);
        }
    }
}

void GraphicsWindow::StartDraggingBySelection() {
    List<Selection> *ls = &(selection);
    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
        if(!s->entity.v) continue;

        StartDraggingByEntity(s->entity);
    }
    // The user might select a point, and then click it again to start
    // dragging; but the point just got unselected by that click. So drag
    // the hovered item too, and they'll always have it.
    if(hover.entity.v) {
        hEntity dragEntity = ChooseFromHoverToDrag().entity;
        if(dragEntity.v != Entity::NO_ENTITY.v) {
            StartDraggingByEntity(dragEntity);
        }
    }
}

void GraphicsWindow::MouseMoved(double x, double y, bool leftDown,
            bool middleDown, bool rightDown, bool shiftDown, bool ctrlDown)
{
    if(GraphicsEditControlIsVisible()) return;
    if(context.active) return;

    SS.extraLine.draw = false;

    if(!orig.mouseDown) {
        // If someone drags the mouse into our window with the left button
        // already depressed, then we don't have our starting point; so
        // don't try.
        leftDown = false;
    }

    if(rightDown) {
        middleDown = true;
        shiftDown = !shiftDown;
    }

    if(SS.showToolbar) {
        if(ToolbarMouseMoved((int)x, (int)y)) {
            hover.Clear();
            return;
        }
    }

    if(!leftDown && (pending.operation == Pending::DRAGGING_POINTS ||
                     pending.operation == Pending::DRAGGING_MARQUEE))
    {
        ClearPending();
        InvalidateGraphics();
    }

    Point2d mp = Point2d::From(x, y);
    currentMousePosition = mp;

    if(rightDown && orig.mouse.DistanceTo(mp) < 5 && !orig.startedMoving) {
        // Avoid accidentally panning (or rotating if shift is down) if the
        // user wants a context menu.
        return;
    }
    orig.startedMoving = true;

    // If the middle button is down, then mouse movement is used to pan and
    // rotate our view. This wins over everything else.
    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

        if(!(shiftDown || ctrlDown)) {
            double s = 0.3*(PI/180)*scale; // degrees per pixel
            projRight = orig.projRight.RotatedAbout(orig.projUp, -s*dx);
            projUp = orig.projUp.RotatedAbout(orig.projRight, s*dy);

            NormalizeProjectionVectors();
        } else if(ctrlDown) {
            double theta = atan2(orig.mouse.y, orig.mouse.x);
            theta -= atan2(y, x);
            SS.extraLine.draw = true;
            SS.extraLine.ptA = UnProjectPoint(Point2d::From(0, 0));
            SS.extraLine.ptB = UnProjectPoint(mp);

            Vector normal = orig.projRight.Cross(orig.projUp);
            projRight = orig.projRight.RotatedAbout(normal, theta);
            projUp = orig.projUp.RotatedAbout(normal, theta);

            NormalizeProjectionVectors();
        } else {
            offset.x = orig.offset.x + dx*projRight.x + dy*projUp.x;
            offset.y = orig.offset.y + dx*projRight.y + dy*projUp.y;
            offset.z = orig.offset.z + dx*projRight.z + dy*projUp.z;
        }

        orig.projRight = projRight;
        orig.projUp = projUp;
        orig.offset = offset;
        orig.mouse.x = x;
        orig.mouse.y = y;

        if(SS.TW.shown.screen == TextWindow::Screen::EDIT_VIEW) {
            if(havePainted) {
                SS.ScheduleShowTW();
            }
        }
        InvalidateGraphics();
        havePainted = false;
        return;
    }

    if(pending.operation == Pending::NONE) {
        double dm = orig.mouse.DistanceTo(mp);
        // If we're currently not doing anything, then see if we should
        // start dragging something.
        if(leftDown && dm > 3) {
            Entity *e = NULL;
            hEntity dragEntity = ChooseFromHoverToDrag().entity;
            if(dragEntity.v) e = SK.GetEntity(dragEntity);
            if(e && e->type != Entity::Type::WORKPLANE) {
                Entity *e = SK.GetEntity(dragEntity);
                if(e->type == Entity::Type::CIRCLE && selection.n <= 1) {
                    // Drag the radius.
                    ClearSelection();
                    pending.circle = dragEntity;
                    pending.operation = Pending::DRAGGING_RADIUS;
                } else if(e->IsNormal()) {
                    ClearSelection();
                    pending.normal = dragEntity;
                    pending.operation = Pending::DRAGGING_NORMAL;
                } else {
                    if(!hoverWasSelectedOnMousedown) {
                        // The user clicked an unselected entity, which
                        // means they're dragging just the hovered thing,
                        // not the full selection. So clear all the selection
                        // except that entity.
                        ClearSelection();
                        MakeSelected(e->h);
                    }
                    StartDraggingBySelection();
                    if(!hoverWasSelectedOnMousedown) {
                        // And then clear the selection again, since they
                        // probably didn't want that selected if they just
                        // were dragging it.
                        ClearSelection();
                    }
                    hover.Clear();
                    pending.operation = Pending::DRAGGING_POINTS;
                }
            } else if(hover.constraint.v &&
                            SK.GetConstraint(hover.constraint)->HasLabel())
            {
                ClearSelection();
                pending.constraint = hover.constraint;
                pending.operation = Pending::DRAGGING_CONSTRAINT;
            }
            if(pending.operation != Pending::NONE) {
                // We just started a drag, so remember for the undo before
                // the drag changes anything.
                SS.UndoRemember();
            } else {
                if(!hover.constraint.v) {
                    // That's just marquee selection, which should not cause
                    // an undo remember.
                    if(dm > 10) {
                        if(hover.entity.v) {
                            // Avoid accidentally selecting workplanes when
                            // starting drags.
                            MakeUnselected(hover.entity, /*coincidentPointTrick=*/false);
                            hover.Clear();
                        }
                        pending.operation = Pending::DRAGGING_MARQUEE;
                        orig.marqueePoint =
                            UnProjectPoint(orig.mouseOnButtonDown);
                    }
                }
            }
        } else {
            // Otherwise, just hit test and give up; but don't hit test
            // if the mouse is down, because then the user could hover
            // a point, mouse down (thus selecting it), and drag, in an
            // effort to drag the point, but instead hover a different
            // entity before we move far enough to start the drag.
            if(!leftDown) {
                // Hit testing can potentially take a lot of time.
                // If we haven't painted since last time we highlighted
                // something, don't hit test again, since this just causes
                // a lag.
                if(!havePainted) return;
                HitTestMakeSelection(mp);
            }
        }
        return;
    }

    // If the user has started an operation from the menu, but not
    // completed it, then just do the selection.
    if(pending.operation == Pending::COMMAND) {
        HitTestMakeSelection(mp);
        return;
    }

    // We're currently dragging something; so do that. But if we haven't
    // painted since the last time we solved, do nothing, because there's
    // no sense solving a frame and not displaying it.
    if(!havePainted) {
        if(pending.operation == Pending::DRAGGING_POINTS && ctrlDown) {
            SS.extraLine.ptA = UnProjectPoint(orig.mouseOnButtonDown);
            SS.extraLine.ptB = UnProjectPoint(mp);
            SS.extraLine.draw = true;
        }
        return;
    }

    havePainted = false;
    switch(pending.operation) {
        case Pending::DRAGGING_CONSTRAINT: {
            Constraint *c = SK.constraint.FindById(pending.constraint);
            UpdateDraggedNum(&(c->disp.offset), x, y);
            orig.mouse = mp;
            InvalidateGraphics();
            return;
        }

        case Pending::DRAGGING_POINTS:
            if(shiftDown || ctrlDown) {
                // Edit the rotation associated with a POINT_N_ROT_TRANS,
                // either within (ctrlDown) or out of (shiftDown) the plane
                // of the screen. So first get the rotation to apply, in qt.
                Quaternion qt;
                if(ctrlDown) {
                    double d = mp.DistanceTo(orig.mouseOnButtonDown);
                    if(d < 25) {
                        // Don't start dragging the position about the normal
                        // until we're a little ways out, to get a reasonable
                        // reference pos
                        orig.mouse = mp;
                        break;
                    }
                    double theta = atan2(orig.mouse.y-orig.mouseOnButtonDown.y,
                                         orig.mouse.x-orig.mouseOnButtonDown.x);
                    theta -= atan2(y-orig.mouseOnButtonDown.y,
                                   x-orig.mouseOnButtonDown.x);

                    Vector gn = projRight.Cross(projUp);
                    qt = Quaternion::From(gn, -theta);

                    SS.extraLine.draw = true;
                    SS.extraLine.ptA = UnProjectPoint(orig.mouseOnButtonDown);
                    SS.extraLine.ptB = UnProjectPoint(mp);
                } else {
                    double dx = -(x - orig.mouse.x);
                    double dy = -(y - orig.mouse.y);
                    double s = 0.3*(PI/180); // degrees per pixel
                    qt = Quaternion::From(projUp,   -s*dx).Times(
                         Quaternion::From(projRight, s*dy));
                }
                orig.mouse = mp;

                // Now apply this rotation to the points being dragged.
                List<hEntity> *lhe = &(pending.points);
                for(hEntity *he = lhe->First(); he; he = lhe->NextAfter(he)) {
                    Entity *e = SK.GetEntity(*he);
                    if(e->type != Entity::Type::POINT_N_ROT_TRANS) {
                        if(ctrlDown) {
                            Vector p = e->PointGetNum();
                            p = p.Minus(SS.extraLine.ptA);
                            p = qt.Rotate(p);
                            p = p.Plus(SS.extraLine.ptA);
                            e->PointForceTo(p);
                            SS.MarkGroupDirtyByEntity(e->h);
                        }
                        continue;
                    }

                    Quaternion q = e->PointGetQuaternion();
                    Vector     p = e->PointGetNum();
                    q = qt.Times(q);
                    e->PointForceQuaternionTo(q);
                    // Let's rotate about the selected point; so fix up the
                    // translation so that that point didn't move.
                    e->PointForceTo(p);
                    SS.MarkGroupDirtyByEntity(e->h);
                }
            } else {
                List<hEntity> *lhe = &(pending.points);
                for(hEntity *he = lhe->First(); he; he = lhe->NextAfter(he)) {
                    UpdateDraggedPoint(*he, x, y);
                    SS.MarkGroupDirtyByEntity(*he);
                }
                orig.mouse = mp;
            }
            break;

        case Pending::DRAGGING_RADIUS: {
            Entity *circle = SK.GetEntity(pending.circle);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            Point2d c2 = ProjectPoint(center);
            double r = c2.DistanceTo(mp)/scale;
            SK.GetEntity(circle->distance)->DistanceForceTo(r);

            SS.MarkGroupDirtyByEntity(pending.circle);
            break;
        }

        case Pending::DRAGGING_NORMAL: {
            Entity *normal = SK.GetEntity(pending.normal);
            Vector p = SK.GetEntity(normal->point[0])->PointGetNum();
            Point2d p2 = ProjectPoint(p);

            Quaternion q = normal->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();

            if(ctrlDown) {
                double theta = atan2(orig.mouse.y-p2.y, orig.mouse.x-p2.x);
                theta -= atan2(y-p2.y, x-p2.x);

                Vector normal = projRight.Cross(projUp);
                u = u.RotatedAbout(normal, -theta);
                v = v.RotatedAbout(normal, -theta);
            } else {
                double dx = -(x - orig.mouse.x);
                double dy = -(y - orig.mouse.y);
                double s = 0.3*(PI/180); // degrees per pixel
                u = u.RotatedAbout(projUp, -s*dx);
                u = u.RotatedAbout(projRight, s*dy);
                v = v.RotatedAbout(projUp, -s*dx);
                v = v.RotatedAbout(projRight, s*dy);
            }
            orig.mouse = mp;
            normal->NormalForceTo(Quaternion::From(u, v));

            SS.MarkGroupDirtyByEntity(pending.normal);
            break;
        }

        case Pending::DRAGGING_MARQUEE:
            orig.mouse = mp;
            InvalidateGraphics();
            return;

        case Pending::NONE:
        case Pending::COMMAND:
            ssassert(false, "Unexpected pending operation");
        default: {
            if (pending.command_c != nullptr) {
                pending.command_c->MouseMoved(mp, leftDown, middleDown,
                    rightDown, shiftDown, ctrlDown);
            }
        }
    }
}

void GraphicsWindow::ClearPending() {
    pending.points.Clear();
    pending.requests.Clear();
    pending = {};
    SS.ScheduleShowTW();
}

bool GraphicsWindow::IsFromPending(hRequest r) {
    for(auto &req : pending.requests) {
        if(req.v == r.v) return true;
    }
    return false;
}

void GraphicsWindow::AddToPending(hRequest r) {
    pending.requests.Add(&r);
}

void GraphicsWindow::ReplacePending(hRequest before, hRequest after) {
    for(auto &req : pending.requests) {
        if(req.v == before.v) {
            req.v = after.v;
        }
    }
}

void GraphicsWindow::MouseMiddleOrRightDown(double x, double y) {
    if(GraphicsEditControlIsVisible()) return;

    orig.offset = offset;
    orig.projUp = projUp;
    orig.projRight = projRight;
    orig.mouse.x = x;
    orig.mouse.y = y;
    orig.startedMoving = false;
}

void GraphicsWindow::ContextMenuListStyles() {
    CreateContextSubmenu();
    Style *s;
    bool empty = true;
    for(s = SK.style.First(); s; s = SK.style.NextAfter(s)) {
        if(s->h.v < Style::FIRST_CUSTOM) continue;

        AddContextMenuItem(s->DescriptionString().c_str(),
                           (ContextCommand)((uint32_t)ContextCommand::FIRST_STYLE + s->h.v));
        empty = false;
    }

    if(!empty) AddContextMenuItem(NULL, ContextCommand::SEPARATOR);

    AddContextMenuItem(_("No Style"), ContextCommand::NO_STYLE);
    AddContextMenuItem(_("Newly Created Custom Style..."), ContextCommand::NEW_CUSTOM_STYLE);
}

void GraphicsWindow::MouseRightUp(double x, double y) {
    SS.extraLine.draw = false;
    InvalidateGraphics();

    // Don't show a context menu if the user is right-clicking the toolbar,
    // or if they are finishing a pan.
    if(ToolbarMouseMoved((int)x, (int)y)) return;
    if(orig.startedMoving) return;

    if(context.active) return;

    //FIXME: wtffff
    if(pending.operation == Pending::DRAGGING_NEW_LINE_POINT ||
       pending.operation == Pending::DRAGGING_NEW_CUBIC_POINT)
    {
        Point2d mp = Point2d::From(x, y);
        pending.command_c->MouseRightUp(mp);
        return;
    }

    // The current mouse location
    Vector v = offset.ScaledBy(-1);
    v = v.Plus(projRight.ScaledBy(x/scale));
    v = v.Plus(projUp.ScaledBy(y/scale));

    context.active = true;

    if(!hover.IsEmpty()) {
        MakeSelected(&hover);
        SS.ScheduleShowTW();
    }
    GroupSelection();

    bool itemsSelected = (gs.n > 0 || gs.constraints > 0);
    int addAfterPoint = -1;

    if(itemsSelected) {
        if(gs.stylables > 0) {
            ContextMenuListStyles();
            AddContextMenuItem(_("Assign to Style"), ContextCommand::SUBMENU);
        }
        if(gs.n + gs.constraints == 1) {
            AddContextMenuItem(_("Group Info"), ContextCommand::GROUP_INFO);
        }
        if(gs.n + gs.constraints == 1 && gs.stylables == 1) {
            AddContextMenuItem(_("Style Info"), ContextCommand::STYLE_INFO);
        }
        if(gs.withEndpoints > 0) {
            AddContextMenuItem(_("Select Edge Chain"), ContextCommand::SELECT_CHAIN);
        }
        if(gs.constraints == 1 && gs.n == 0) {
            Constraint *c = SK.GetConstraint(gs.constraint[0]);
            if(c->HasLabel() && c->type != Constraint::Type::COMMENT) {
                AddContextMenuItem(_("Toggle Reference Dimension"),
                    ContextCommand::REFERENCE_DIM);
            }
            if(c->type == Constraint::Type::ANGLE ||
               c->type == Constraint::Type::EQUAL_ANGLE)
            {
                AddContextMenuItem(_("Other Supplementary Angle"),
                    ContextCommand::OTHER_ANGLE);
            }
        }
        if(gs.constraintLabels > 0 || gs.points > 0) {
            AddContextMenuItem(_("Snap to Grid"), ContextCommand::SNAP_TO_GRID);
        }

        if(gs.points == 1 && gs.point[0].isFromRequest()) {
            Request *r = SK.GetRequest(gs.point[0].request());
            int index = r->IndexOfPoint(gs.point[0]);
            if((r->type == Request::Type::CUBIC && (index > 1 && index < r->extraPoints + 2)) ||
                    r->type == Request::Type::CUBIC_PERIODIC) {
                AddContextMenuItem(_("Remove Spline Point"), ContextCommand::REMOVE_SPLINE_PT);
            }
        }
        if(gs.entities == 1 && gs.entity[0].isFromRequest()) {
            Request *r = SK.GetRequest(gs.entity[0].request());
            if(r->type == Request::Type::CUBIC || r->type == Request::Type::CUBIC_PERIODIC) {
                Entity *e = SK.GetEntity(gs.entity[0]);
                addAfterPoint = e->GetPositionOfPoint(GetCamera(), Point2d::From(x, y));
                ssassert(addAfterPoint != -1, "Expected a nearest bezier point to be located");
                // Skip derivative point.
                if(r->type == Request::Type::CUBIC) addAfterPoint++;
                AddContextMenuItem(_("Add Spline Point"), ContextCommand::ADD_SPLINE_PT);
            }
        }
        if(gs.entities == gs.n) {
            AddContextMenuItem(_("Toggle Construction"), ContextCommand::CONSTRUCTION);
        }

        if(gs.points == 1) {
            Entity *p = SK.GetEntity(gs.point[0]);
            Constraint *c;
            IdList<Constraint,hConstraint> *lc = &(SK.constraint);
            for(c = lc->First(); c; c = lc->NextAfter(c)) {
                if(c->type != Constraint::Type::POINTS_COINCIDENT) continue;
                if(c->ptA.v == p->h.v || c->ptB.v == p->h.v) {
                    break;
                }
            }
            if(c) {
                AddContextMenuItem(_("Delete Point-Coincident Constraint"),
                                   ContextCommand::DEL_COINCIDENT);
            }
        }
        AddContextMenuItem(NULL, ContextCommand::SEPARATOR);
        if(LockedInWorkplane()) {
            AddContextMenuItem(_("Cut"),  ContextCommand::CUT_SEL);
            AddContextMenuItem(_("Copy"), ContextCommand::COPY_SEL);
        }
    } else {
        AddContextMenuItem(_("Select All"), ContextCommand::SELECT_ALL);
    }

    if((SS.clipboard.r.n > 0 || SS.clipboard.c.n > 0) && LockedInWorkplane()) {
        AddContextMenuItem(_("Paste"), ContextCommand::PASTE);
        AddContextMenuItem(_("Paste Transformed..."), ContextCommand::PASTE_XFRM);
    }

    if(itemsSelected) {
        AddContextMenuItem(_("Delete"), ContextCommand::DELETE_SEL);
        AddContextMenuItem(NULL, ContextCommand::SEPARATOR);
        AddContextMenuItem(_("Unselect All"), ContextCommand::UNSELECT_ALL);
    }
    // If only one item is selected, then it must be the one that we just
    // selected from the hovered item; in which case unselect all and hovered
    // are equivalent.
    if(!hover.IsEmpty() && selection.n > 1) {
        AddContextMenuItem(_("Unselect Hovered"), ContextCommand::UNSELECT_HOVERED);
    }

    if(itemsSelected) {
        AddContextMenuItem(NULL, ContextCommand::SEPARATOR);
        AddContextMenuItem(_("Zoom to Fit"), ContextCommand::ZOOM_TO_FIT);
    }

    ContextCommand ret = ShowContextMenu();
    switch(ret) {
        case ContextCommand::CANCELLED:
            // otherwise it was cancelled, so do nothing
            contextMenuCancelTime = GetMilliseconds();
            break;

        case ContextCommand::UNSELECT_ALL:
            MenuEdit(Command::UNSELECT_ALL);
            break;

        case ContextCommand::UNSELECT_HOVERED:
            if(!hover.IsEmpty()) {
                MakeUnselected(&hover, /*coincidentPointTrick=*/true);
            }
            break;

        case ContextCommand::SELECT_CHAIN:
            MenuEdit(Command::SELECT_CHAIN);
            break;

        case ContextCommand::CUT_SEL:
            MenuClipboard(Command::CUT);
            break;

        case ContextCommand::COPY_SEL:
            MenuClipboard(Command::COPY);
            break;

        case ContextCommand::PASTE:
            MenuClipboard(Command::PASTE);
            break;

        case ContextCommand::PASTE_XFRM:
            MenuClipboard(Command::PASTE_TRANSFORM);
            break;

        case ContextCommand::DELETE_SEL:
            MenuClipboard(Command::DELETE);
            break;

        case ContextCommand::REFERENCE_DIM:
            Constraint::MenuConstrain(Command::REFERENCE);
            break;

        case ContextCommand::OTHER_ANGLE:
            Constraint::MenuConstrain(Command::OTHER_ANGLE);
            break;

        case ContextCommand::DEL_COINCIDENT: {
            SS.UndoRemember();
            if(!gs.point[0].v) break;
            Entity *p = SK.GetEntity(gs.point[0]);
            if(!p->IsPoint()) break;

            SK.constraint.ClearTags();
            Constraint *c;
            for(c = SK.constraint.First(); c; c = SK.constraint.NextAfter(c)) {
                if(c->type != Constraint::Type::POINTS_COINCIDENT) continue;
                if(c->ptA.v == p->h.v || c->ptB.v == p->h.v) {
                    c->tag = 1;
                }
            }
            SK.constraint.RemoveTagged();
            ClearSelection();
            break;
        }

        case ContextCommand::SNAP_TO_GRID:
            MenuEdit(Command::SNAP_TO_GRID);
            break;

        case ContextCommand::CONSTRUCTION:
            MenuRequest(Command::CONSTRUCTION);
            break;

        case ContextCommand::ZOOM_TO_FIT:
            MenuView(Command::ZOOM_TO_FIT);
            break;

        case ContextCommand::SELECT_ALL:
            MenuEdit(Command::SELECT_ALL);
            break;

        case ContextCommand::REMOVE_SPLINE_PT: {
            hRequest hr = gs.point[0].request();
            Request *r = SK.GetRequest(hr);

            int index = r->IndexOfPoint(gs.point[0]);
            ssassert(r->extraPoints != 0, "Expected a bezier with interior control points");

            SS.UndoRemember();
            Entity *e = SK.GetEntity(r->h.entity(0));

            // First, fix point-coincident constraints involving this point.
            // Then, remove all other constraints, since they would otherwise
            // jump to an adjacent one and mess up the bezier after generation.
            FixConstraintsForPointBeingDeleted(e->point[index]);
            RemoveConstraintsForPointBeingDeleted(e->point[index]);

            for(int i = index; i < MAX_POINTS_IN_ENTITY - 1; i++) {
                if(e->point[i + 1].v == 0) break;
                Entity *p0 = SK.GetEntity(e->point[i]);
                Entity *p1 = SK.GetEntity(e->point[i + 1]);
                ReplacePointInConstraints(p1->h, p0->h);
                p0->PointForceTo(p1->PointGetNum());
            }
            r->extraPoints--;
            SS.MarkGroupDirtyByEntity(gs.point[0]);
            ClearSelection();
            break;
        }

        case ContextCommand::ADD_SPLINE_PT: {
            hRequest hr = gs.entity[0].request();
            Request *r = SK.GetRequest(hr);

            int pointCount = r->extraPoints + ((r->type == Request::Type::CUBIC_PERIODIC) ? 3 : 4);
            if(pointCount < MAX_POINTS_IN_ENTITY) {
                SS.UndoRemember();
                r->extraPoints++;
                SS.MarkGroupDirtyByEntity(gs.entity[0]);
                SS.GenerateAll(SolveSpaceUI::Generate::REGEN);

                Entity *e = SK.GetEntity(r->h.entity(0));
                for(int i = MAX_POINTS_IN_ENTITY; i > addAfterPoint + 1; i--) {
                    Entity *p0 = SK.entity.FindByIdNoOops(e->point[i]);
                    if(p0 == NULL) continue;
                    Entity *p1 = SK.GetEntity(e->point[i - 1]);
                    ReplacePointInConstraints(p1->h, p0->h);
                    p0->PointForceTo(p1->PointGetNum());
                }
                Entity *p = SK.GetEntity(e->point[addAfterPoint + 1]);
                p->PointForceTo(v);
                SS.MarkGroupDirtyByEntity(gs.entity[0]);
                ClearSelection();
            } else {
                Error(_("Cannot add spline point: maximum number of points reached."));
            }
            break;
        }

        case ContextCommand::GROUP_INFO: {
            hGroup hg;
            if(gs.entities == 1) {
                hg = SK.GetEntity(gs.entity[0])->group;
            } else if(gs.points == 1) {
                hg = SK.GetEntity(gs.point[0])->group;
            } else if(gs.constraints == 1) {
                hg = SK.GetConstraint(gs.constraint[0])->group;
            } else {
                break;
            }
            ClearSelection();

            SS.TW.GoToScreen(TextWindow::Screen::GROUP_INFO);
            SS.TW.shown.group = hg;
            SS.ScheduleShowTW();
            ForceTextWindowShown();
            break;
        }

        case ContextCommand::STYLE_INFO: {
            hStyle hs;
            if(gs.entities == 1) {
                hs = Style::ForEntity(gs.entity[0]);
            } else if(gs.points == 1) {
                hs = Style::ForEntity(gs.point[0]);
            } else if(gs.constraints == 1) {
                hs = SK.GetConstraint(gs.constraint[0])->GetStyle();
            } else {
                break;
            }
            ClearSelection();

            SS.TW.GoToScreen(TextWindow::Screen::STYLE_INFO);
            SS.TW.shown.style = hs;
            SS.ScheduleShowTW();
            ForceTextWindowShown();
            break;
        }

        case ContextCommand::NEW_CUSTOM_STYLE: {
            uint32_t v = Style::CreateCustomStyle();
            Style::AssignSelectionToStyle(v);
            ForceTextWindowShown();
            break;
        }

        case ContextCommand::NO_STYLE:
            Style::AssignSelectionToStyle(0);
            break;

        default:
            ssassert(ret >= ContextCommand::FIRST_STYLE, "Expected a style to be chosen");
            Style::AssignSelectionToStyle((uint32_t)ret - (uint32_t)ContextCommand::FIRST_STYLE);
            break;
    }

    context.active = false;
    SS.ScheduleShowTW();
}

hRequest GraphicsWindow::AddRequest(Request::Type type) {
    return AddRequest(type, /*rememberForUndo=*/true);
}
hRequest GraphicsWindow::AddRequest(Request::Type type, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    Request r = {};
    r.group = activeGroup;
    Group *g = SK.GetGroup(activeGroup);
    if(g->type == Group::Type::DRAWING_3D || g->type == Group::Type::DRAWING_WORKPLANE) {
        r.construction = false;
    } else {
        r.construction = true;
    }
    r.workplane = ActiveWorkplane();
    r.type = type;
    SK.request.AddAndAssignId(&r);

    // We must regenerate the parameters, so that the code that tries to
    // place this request's entities where the mouse is can do so. But
    // we mustn't try to solve until reasonable values have been supplied
    // for these new parameters, or else we'll get a numerical blowup.
    r.Generate(&SK.entity, &SK.param);
    SS.MarkGroupDirty(r.group);
    return r.h;
}

bool GraphicsWindow::ConstrainPointByHovered(hEntity pt) {
    if(!hover.entity.v) return false;

    Entity *e = SK.GetEntity(hover.entity);
    if(e->IsPoint()) {
        Entity *point = SK.GetEntity(pt);
        point->PointForceTo(e->PointGetNum());
        Constraint::ConstrainCoincident(e->h, pt);
        return true;
    }
    if(e->IsCircle()) {
        Constraint::Constrain(Constraint::Type::PT_ON_CIRCLE,
            pt, Entity::NO_ENTITY, e->h);
        return true;
    }
    if(e->type == Entity::Type::LINE_SEGMENT) {
        Constraint::Constrain(Constraint::Type::PT_ON_LINE,
            pt, Entity::NO_ENTITY, e->h);
        return true;
    }

    return false;
}

void GraphicsWindow::MouseLeftDown(double mx, double my) {
    orig.mouseDown = true;

    if(GraphicsEditControlIsVisible()) {
        orig.mouse = Point2d::From(mx, my);
        orig.mouseOnButtonDown = orig.mouse;
        HideGraphicsEditControl();
        return;
    }
    SS.TW.HideEditControl();

    if(SS.showToolbar) {
        if(ToolbarMouseDown((int)mx, (int)my)) return;
    }

    // This will be clobbered by MouseMoved below.
    bool hasConstraintSuggestion = SS.GW.pending.hasSuggestion;

    // Make sure the hover is up to date.
    MouseMoved(mx, my, /*leftDown=*/false, /*middleDown=*/false, /*rightDown=*/false,
        /*shiftDown=*/false, /*ctrlDown=*/false);
    orig.mouse.x = mx;
    orig.mouse.y = my;
    orig.mouseOnButtonDown = orig.mouse;
    
    SS.GW.pending.hasSuggestion = hasConstraintSuggestion;

    // The current mouse location
    Vector v = offset.ScaledBy(-1);
    v = v.Plus(projRight.ScaledBy(mx/scale));
    v = v.Plus(projUp.ScaledBy(my/scale));

    if (pending.command_c) {
        pending.command_c->MouseLeftDown(v);
    } else {
        ClearPending();
        if(!hover.IsEmpty()) {
            SS.GW.hoverWasSelectedOnMousedown = SS.GW.IsSelected(&hover);
            SS.GW.MakeSelected(&hover);
        }
    }

    SS.ScheduleShowTW();
    InvalidateGraphics();
}

void GraphicsWindow::MouseLeftUp(double mx, double my) {
    orig.mouseDown = false;
    hoverWasSelectedOnMousedown = false;

    switch(pending.operation) {
        case Pending::DRAGGING_POINTS:
            SS.extraLine.draw = false;
            // fall through
        case Pending::DRAGGING_CONSTRAINT:
        case Pending::DRAGGING_NORMAL:
        case Pending::DRAGGING_RADIUS:
            ClearPending();
            InvalidateGraphics();
            break;

        case Pending::DRAGGING_MARQUEE:
            SelectByMarquee();
            ClearPending();
            InvalidateGraphics();
            break;

        case Pending::NONE:
            // We need to clear the selection here, and not in the mouse down
            // event, since a mouse down without anything hovered could also
            // be the start of marquee selection. But don't do that on the
            // left click to cancel a context menu. The time delay is an ugly
            // hack.
            if(hover.IsEmpty() &&
                (contextMenuCancelTime == 0 ||
                 (GetMilliseconds() - contextMenuCancelTime) > 200))
            {
                ClearSelection();
            }
            break;

        default:
            break;  // do nothing
    }
}

void GraphicsWindow::MouseLeftDoubleClick(double mx, double my) {
    if(GraphicsEditControlIsVisible()) return;
    SS.TW.HideEditControl();

    if(hover.constraint.v) {
        constraintBeingEdited = hover.constraint;
        ClearSuper();

        Constraint *c = SK.GetConstraint(constraintBeingEdited);
        if(!c->HasLabel()) {
            // Not meaningful to edit a constraint without a dimension
            return;
        }
        if(c->reference) {
            // Not meaningful to edit a reference dimension
            return;
        }

        Vector p3 = c->GetLabelPos(GetCamera());
        Point2d p2 = ProjectPoint(p3);

        std::string editValue;
        int editMinWidthChar;
        switch(c->type) {
            case Constraint::Type::COMMENT:
                editValue = c->comment;
                editMinWidthChar = 30;
                break;

            case Constraint::Type::ANGLE:
            case Constraint::Type::LENGTH_RATIO:
                editValue = ssprintf("%.3f", c->valA);
                editMinWidthChar = 5;
                break;

            default: {
                double v = fabs(c->valA);

                // If displayed as radius, also edit as radius.
                if(c->type == Constraint::Type::DIAMETER && c->other)
                    v /= 2;

                std::string def = SS.MmToString(v);
                double eps = 1e-12;
                if(fabs(SS.StringToMm(def) - v) < eps) {
                    // Show value with default number of digits after decimal,
                    // which is at least enough to represent it exactly.
                    editValue = def;
                } else {
                    // Show value with as many digits after decimal as
                    // required to represent it exactly, up to 10.
                    v /= SS.MmPerUnit();
                    int i;
                    for(i = 0; i <= 10; i++) {
                        editValue = ssprintf("%.*f", i, v);
                        if(fabs(std::stod(editValue) - v) < eps) break;
                    }
                }
                editMinWidthChar = 5;
                break;
            }
        }
        hStyle hs = c->disp.style;
        if(hs.v == 0) hs.v = Style::CONSTRAINT;
        ShowGraphicsEditControl((int)p2.x, (int)p2.y,
                                (int)(VectorFont::Builtin()->GetHeight(Style::TextHeight(hs))),
                                editMinWidthChar, editValue);
    }
}

void GraphicsWindow::EditControlDone(const char *s) {
    HideGraphicsEditControl();
    Constraint *c = SK.GetConstraint(constraintBeingEdited);

    if(c->type == Constraint::Type::COMMENT) {
        SS.UndoRemember();
        c->comment = s;
        return;
    }

    Expr *e = Expr::From(s, true);
    if(e) {
        SS.UndoRemember();

        switch(c->type) {
            case Constraint::Type::PROJ_PT_DISTANCE:
            case Constraint::Type::PT_LINE_DISTANCE:
            case Constraint::Type::PT_FACE_DISTANCE:
            case Constraint::Type::PT_PLANE_DISTANCE:
            case Constraint::Type::LENGTH_DIFFERENCE: {
                // The sign is not displayed to the user, but this is a signed
                // distance internally. To flip the sign, the user enters a
                // negative distance.
                bool wasNeg = (c->valA < 0);
                if(wasNeg) {
                    c->valA = -SS.ExprToMm(e);
                } else {
                    c->valA = SS.ExprToMm(e);
                }
                break;
            }
            case Constraint::Type::ANGLE:
            case Constraint::Type::LENGTH_RATIO:
                // These don't get the units conversion for distance, and
                // they're always positive
                c->valA = fabs(e->Eval());
                break;

            case Constraint::Type::DIAMETER:
                c->valA = fabs(SS.ExprToMm(e));

                // If displayed and edited as radius, convert back
                // to diameter
                if(c->other)
                    c->valA *= 2;
                break;

            default:
                // These are always positive, and they get the units conversion.
                c->valA = fabs(SS.ExprToMm(e));
                break;
        }
        SS.MarkGroupDirty(c->group);
    }
}

bool GraphicsWindow::KeyDown(int c) {
    if(c == '\b') {
        // Treat backspace identically to escape.
        MenuEdit(Command::UNSELECT_ALL);
        return true;
    } else if(c == '=') {
        // Treat = as +. This is specific to US (and US-compatible) keyboard layouts,
        // but makes zooming from keyboard much more usable on these.
        // Ideally we'd have a platform-independent way of binding to a particular
        // physical key regardless of shift status...
        MenuView(Command::ZOOM_IN);
        return true;
    }

    return false;
}

void GraphicsWindow::MouseScroll(double x, double y, int delta) {
    double offsetRight = offset.Dot(projRight);
    double offsetUp = offset.Dot(projUp);

    double righti = x/scale - offsetRight;
    double upi = y/scale - offsetUp;

    if(delta > 0) {
        scale *= 1.2;
    } else if(delta < 0) {
        scale /= 1.2;
    } else return;

    double rightf = x/scale - offsetRight;
    double upf = y/scale - offsetUp;

    offset = offset.Plus(projRight.ScaledBy(rightf - righti));
    offset = offset.Plus(projUp.ScaledBy(upf - upi));

    if(SS.TW.shown.screen == TextWindow::Screen::EDIT_VIEW) {
        if(havePainted) {
            SS.ScheduleShowTW();
        }
    }
    havePainted = false;
    InvalidateGraphics();
}

void GraphicsWindow::MouseLeave() {
    // Un-hover everything when the mouse leaves our window, unless there's
    // currently a context menu shown.
    if(!context.active) {
        hover.Clear();
        toolbarTooltipped = Command::NONE;
        toolbarHovered = Command::NONE;
        PaintGraphics();
    }
    SS.extraLine.draw = false;
}

void GraphicsWindow::SpaceNavigatorMoved(double tx, double ty, double tz,
                                         double rx, double ry, double rz,
                                         bool shiftDown)
{
    if(!havePainted) return;
    Vector out = projRight.Cross(projUp);

    // rotation vector is axis of rotation, and its magnitude is angle
    Vector aa = Vector::From(rx, ry, rz);
    // but it's given with respect to screen projection frame
    aa = aa.ScaleOutOfCsys(projRight, projUp, out);
    double aam = aa.Magnitude();
    if(aam > 0.0) aa = aa.WithMagnitude(1);

    // This can either transform our view, or transform a linked part.
    GroupSelection();
    Entity *e = NULL;
    Group *g = NULL;
    if(gs.points == 1   && gs.n == 1) e = SK.GetEntity(gs.point [0]);
    if(gs.entities == 1 && gs.n == 1) e = SK.GetEntity(gs.entity[0]);
    if(e) g = SK.GetGroup(e->group);
    if(g && g->type == Group::Type::LINKED && !shiftDown) {
        // Apply the transformation to a linked part. Gain down the Z
        // axis, since it's hard to see what you're doing on that one since
        // it's normal to the screen.
        Vector t = projRight.ScaledBy(tx/scale).Plus(
                   projUp   .ScaledBy(ty/scale).Plus(
                   out      .ScaledBy(0.1*tz/scale)));
        Quaternion q = Quaternion::From(aa, aam);

        // If we go five seconds without SpaceNavigator input, or if we've
        // switched groups, then consider that a new action and save an undo
        // point.
        int64_t now = GetMilliseconds();
        if(now - lastSpaceNavigatorTime > 5000 ||
           lastSpaceNavigatorGroup.v != g->h.v)
        {
            SS.UndoRemember();
        }

        g->TransformImportedBy(t, q);

        lastSpaceNavigatorTime = now;
        lastSpaceNavigatorGroup = g->h;
        SS.MarkGroupDirty(g->h);
    } else {
        // Apply the transformation to the view of the everything. The
        // x and y components are translation; but z component is scale,
        // not translation, or else it would do nothing in a parallel
        // projection
        offset = offset.Plus(projRight.ScaledBy(tx/scale));
        offset = offset.Plus(projUp.ScaledBy(ty/scale));
        scale *= exp(0.001*tz);

        if(aam > 0.0) {
            projRight = projRight.RotatedAbout(aa, -aam);
            projUp    = projUp.   RotatedAbout(aa, -aam);
            NormalizeProjectionVectors();
        }
    }

    havePainted = false;
    InvalidateGraphics();
}

void GraphicsWindow::SpaceNavigatorButtonUp() {
    ZoomToFit(/*includingInvisibles=*/false, /*useSelection=*/true);
    InvalidateGraphics();
}

