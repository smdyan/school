
from sqlmodel import SQLModel, Field
from typing import Optional
from src.model.subject import Subject
from src.model.weekday import Weekday


class LessonBase( SQLModel ):
    weekdayNum: int | None = Field(default=None, foreign_key="weekday.dayNum")
    number: int
    subjectId: int | None = Field(default=None, foreign_key="subject.id")
    subject: Optional[str] = None


class Lesson( LessonBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class LessonCreate( LessonBase ):
    pass


class LessonPublic( LessonBase ):
    id: int
