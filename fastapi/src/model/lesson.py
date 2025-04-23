
from sqlmodel import SQLModel, Field, Relationship
from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from src.model.weekday import Weekday
    from src.model.subject import Subject


class LessonBase( SQLModel ):
    lessonNum: int
    subjectId: int | None = Field(default=None, foreign_key="subject.id")
    weekdayId: int | None = Field(default=None, foreign_key="weekday.id")


class Lesson( LessonBase, table=True ):
    id: int = Field( default=None, primary_key=True )
    weekday: Optional["Weekday"] = Relationship( back_populates="lessons" )
    subject: Optional["Subject"] = Relationship( back_populates="lessons" )


class LessonCreate( LessonBase ):
    pass


class LessonPublic( LessonBase ):
    id: int
