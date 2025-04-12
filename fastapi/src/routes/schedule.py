from typing import Annotated
from fastapi import HTTPException, Query, APIRouter
from sqlmodel import select
from src.data.init import SessionDep
from src.model.schedule import Lesson, LessonCreate, LessonPublic


router = APIRouter( prefix="/sch" )


@router.get("/")
def getSchedule( session: SessionDep ) -> list[Lesson]:
    result = session.exec(select(Lesson)).all()
    return result


@router.get("/wd/{weekday}")
def getSchedule( weekday: str, session: SessionDep ) -> list[Lesson]:
    result = session.exec(select(Lesson).filter_by(weekday=weekday))
    return [
        Lesson( 
            id=lesson.id,
            weekday=lesson.weekday,
            sequenceNum=lesson.sequenceNum,
            subject=lesson.subject ) for lesson in result
    ]

@router.post("/", response_model=LessonPublic)
def addLesson( lesson: LessonCreate, session: SessionDep ):
    db_lesson = Lesson.model_validate(lesson)
    session.add( db_lesson )
    session.commit()
    session.refresh( db_lesson )
    return db_lesson


@router.get("/id/{id}")
async def getLesson(id: int, session: SessionDep) -> Lesson:
    lesson = session.get(Lesson, id)
    if not lesson:
        raise HTTPException(status_code=404, detail="schedule not found")
    return lesson


@router.delete("/{id}")
async def deleteLesson(id: int, session: SessionDep):
    lesson = session.get(Lesson, id)
    if not lesson:
        raise HTTPException(status_code=404, detail="schedule not found")
    session.delete( lesson )
    session.commit()
    return {"ok": True}
