
from src.model.weekday import Weekday, WeekdayCreate
from src.model.subject import Subject, SubjectCreate
from sqlmodel import Session
from fastapi import HTTPException


def createWeekdays( session: Session ):
        week = {1:"Monday", 2:"Tuesday", 3:"Wednesday", 4:"Thursday", 5:"Friday", 6:"Saturday", 7:"Sunday"}
        for day in week:
                weekday = WeekdayCreate( dayNum=day, dayName = week[day] )
                dweekday = Weekday.model_validate( weekday )
                session.add( dweekday )
        session.commit()
        return {"ok": True}

def createSubjects( session: Session ):
        names = ["Matematika", "Russkiy", "Risovanie", "Biology", "Geography", "Music", "Trud"]
        for name in names:
                subject = SubjectCreate( name=name )
                dsubject = Subject.model_validate( subject )
                session.add( dsubject )
        session.commit()
        return {"ok": True}