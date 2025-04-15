from sqlmodel import SQLModel, Field
from typing import Optional


class WeekdayBase( SQLModel ):
    dayNum: int
    dayName: str


class Weekday( WeekdayBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class WeekdayCreate( WeekdayBase ):
    pass
