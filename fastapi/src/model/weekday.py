from sqlmodel import SQLModel, Field, Relationship
from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from src.model.lesson import Lesson


class WeekdayBase( SQLModel ):
    dayNum: int
    dayName: str


class Weekday( WeekdayBase, table=True ):
    id: int = Field( default=None, primary_key=True )
    lessons: Optional[list["Lesson"]] = Relationship( back_populates="weekday" )

class WeekdayCreate( WeekdayBase ):
    pass
