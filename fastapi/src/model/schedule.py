from sqlmodel import SQLModel, Field
from typing import Optional


class LessonBase( SQLModel ):
    weekday: str
    sequenceNum: int
    subject: Optional[str] = None


class Lesson( LessonBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class LessonCreate( LessonBase ):
    pass


class LessonPublic( LessonBase ):
    id: int



class WeekdayBase( SQLModel ):
    sequenceNum: str
    dayName: int


class Weekday( WeekdayBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class WeekdayCreate( WeekdayBase ):
    pass
