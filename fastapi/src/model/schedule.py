
from sqlmodel import SQLModel
from typing import Optional


class Schedule( SQLModel ):
    dayName: str
    lessons: Optional[list[str]] = []
