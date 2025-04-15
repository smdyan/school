
from sqlmodel import SQLModel
from typing import Optional


class Schedule( SQLModel ):
    weekday: Optional[str] = None
    lesson1: Optional[str] = None
    lesson2: Optional[str] = None
    lesson3: Optional[str] = None
    lesson4: Optional[str] = None
    lesson5: Optional[str] = None
    lesson6: Optional[str] = None
