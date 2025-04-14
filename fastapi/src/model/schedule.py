
from sqlmodel import SQLModel, Field
from typing import Optional


class LessonBase( SQLModel ):
    weekdayNum: int | None = Field(default=None, foreign_key="weekday.dayNum")
    weekday: str
    lessonNum: int
    subjectId: int | None = Field(default=None, foreign_key="subject.id")
    subject: Optional[str] = None


class Lesson( LessonBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class LessonCreate( LessonBase ):
    pass


class LessonPublic( LessonBase ):
    id: int



class WeekdayBase( SQLModel ):
    dayNum: int
    dayName: str


class Weekday( WeekdayBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class WeekdayCreate( WeekdayBase ):
    pass


class SubjectBase( SQLModel ):
    Name: str


class Subject( SubjectBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class SubjectCreate( SubjectBase ):
    pass
