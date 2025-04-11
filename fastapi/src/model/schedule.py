from sqlmodel import SQLModel, Field


class ScheduleBase( SQLModel ):
    day: str
    lesson: str


class ScheduleCreate( ScheduleBase ):
    pass


class Schedule( ScheduleBase, table=True ):
    id: int = Field( default=None, primary_key=True )


