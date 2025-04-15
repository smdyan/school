from typing import Annotated
from fastapi import HTTPException, APIRouter
from sqlmodel import select
from src.data.init import SessionDep
from src.model.lesson import Lesson, LessonCreate, LessonPublic
from src.model.weekday import Weekday
from src.model.subject import Subject
from src.model.schedule import Schedule
from src.fake.schedule import createWeekdays, createSubjects


router = APIRouter( prefix="/sch" )

@router.get("/init")
def push2db( session: SessionDep ):
    createWeekdays( session )
    createSubjects ( session )
    return True

# @router.get("/")
# def getSchedule( session: SessionDep ) -> list[Lesson]:
#     # select generates sql request for a table Lesson
#     statement = select( Lesson )
#     # get table object
#     result = session.exec( statement )
#     # create list of instances from iterable
#     lessons = result.all()
#     return lessons


# @router.get("/wd/{weekday}")
# def getSchedule( weekday: str, session: SessionDep ) -> list[Lesson]:
#     result = session.exec(select(Lesson).filter_by(weekday=weekday))
#     return [
#         Lesson( 
#             id=lesson.id,
#             weekday=lesson.weekday,
#             lessonNum=lesson.lessonNum,
#             subject=lesson.subject ) for lesson in result
#     ]

@router.post("/", response_model=LessonPublic)
def addLesson( lesson: LessonCreate, session: SessionDep ):
    dbLesson = Lesson.model_validate( lesson )
    session.add( dbLesson )
    session.commit()
    session.refresh( dbLesson )
    return dbLesson


@router.get("/id/{id}", response_model=LessonPublic)
async def getLesson(id: int, session: SessionDep):
    lesson = session.get(Lesson, id)
    if not lesson:
        raise HTTPException(status_code=404, detail="lesson not found")
    return lesson


@router.delete("/{id}")
async def deleteLesson(id: int, session: SessionDep):
    lesson = session.get(Lesson, id)
    if not lesson:
        raise HTTPException(status_code=404, detail="lesson not found")
    session.delete( lesson )
    session.commit()
    return {"ok": True}


@router.get("/wd/{weekdayId}")
def getSchedule( weekdayId: int, session: SessionDep ) -> Schedule:
    statement = select( Lesson, Weekday.dayName, Subject.Name ).join( Weekday ).join( Subject ).where( Lesson.weekdayNum == weekdayId )
    
    result = session.exec( statement )
    for lesson, weekday, subject in result:
            print( "Lesson:", weekday, lesson.number, subject )
            schedule = Schedule( weekday = weekday, lesson1=subject )

    return schedule
