from sqlmodel import SQLModel, Field, Relationship
from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from src.model.lesson import Lesson

class SubjectBase( SQLModel ):
    name: str


class Subject( SubjectBase, table=True ):
    id: int = Field( default=None, primary_key=True )
    lessons: Optional[list["Lesson"]] = Relationship( back_populates="subject" )


class SubjectCreate( SubjectBase ):
    pass
