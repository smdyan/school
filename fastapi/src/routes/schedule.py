from typing import Annotated
from fastapi import HTTPException, APIRouter
from sqlmodel import select
from src.data.init import SessionDep
from src.model.lesson import Lesson, LessonCreate, LessonPublic
from src.model.weekday import Weekday
from src.model.subject import Subject
from src.model.schedule import Schedule
from src.fake.schedule import createWeekdays, createSubjects


router = APIRouter( prefix="/school" )

@router.get("/init")
def push2db( session: SessionDep ):
    wd1 = session.get( Weekday, 1)
    if wd1:
        raise HTTPException(status_code=404, detail="data set already")
    createWeekdays( session )
    createSubjects ( session )
    return True


@router.post( "/lesson", response_model=LessonPublic )
def addLesson( lesson: LessonCreate, session: SessionDep ):
    dbLesson = Lesson.model_validate( lesson )
    wd = session.get( Weekday,  lesson.weekdayId )
    dbLesson.weekday = wd
    session.add( dbLesson )
    session.commit()
    session.refresh( dbLesson )
    return dbLesson


@router.delete("/lesson/{id}")
async def deleteLesson(id: int, session: SessionDep):
    lesson = session.get(Lesson, id)
    if not lesson:
        raise HTTPException(status_code=404, detail="lesson not found")
    session.delete( lesson )
    session.commit()
    return {"ok": True}


@router.get("/lesson/{id}", response_model=LessonPublic)
async def getLesson(id: int, session: SessionDep):
    lesson = session.get(Lesson, id)
    if not lesson:
        raise HTTPException(status_code=404, detail="lesson not found")
    print( "stdout:", lesson.wee )
    return lesson


@router.get("/lessons")
def getLessons( session: SessionDep ) -> list[Lesson]:
    # select generates sql request for a table Lesson
    statement = select( Lesson )
    # get table object
    result = session.exec( statement )
    # create list of instances from iterable
    lessons = result.all()
    return lessons


@router.get( "/weekdays", response_model=list[Weekday] )
async def getWeeldays( session: SessionDep ):
    weekdays = session.exec( select( Weekday )).all()
    if not weekdays:
        raise HTTPException( status_code=404, detail="weekdays not found" )
    return weekdays


@router.get( "/subjects", response_model=list[Subject] )
async def getSubjects( session: SessionDep ):
    subjects = session.exec( select( Subject )).all()
    if not subjects:
        raise HTTPException( status_code=404, detail="subjects not found" )
    return subjects


@router.get( "/schedule/{num}")
async def getSchedule(num: int, session: SessionDep) -> Schedule:
    # weekday = session.get(Weekday, id)
    weekday = session.exec( select( Weekday ).where( Weekday.dayNum == num )).one()
    if not weekday:
        raise HTTPException(status_code=404, detail="weekday not found")
    schedule = Schedule( dayName = weekday.dayName )
    for lesson in weekday.lessons:
        schedule.lessons.append( lesson.subject.name )
    return schedule
