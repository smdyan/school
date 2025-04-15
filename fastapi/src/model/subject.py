from sqlmodel import SQLModel, Field
from typing import Optional


class SubjectBase( SQLModel ):
    Name: str


class Subject( SubjectBase, table=True ):
    id: int = Field( default=None, primary_key=True )


class SubjectCreate( SubjectBase ):
    pass
