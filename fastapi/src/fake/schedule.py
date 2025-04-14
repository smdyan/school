
from src.model.schedule import Weekday, WeekdayCreate
from src.model.schedule import Subject, SubjectCreate
from sqlmodel import Session

def createWeekdays( session: Session ):
        day1 = WeekdayCreate( dayNum=1, dayName="Monday" )
        dday1 = Weekday.model_validate( day1 )
        day2 = WeekdayCreate( dayNum=2, dayName="Tuesday" )
        dday2 = Weekday.model_validate( day2 )
        day3 = WeekdayCreate( dayNum=3, dayName="Wednesday" )
        dday3 = Weekday.model_validate( day3 )
        day4 = WeekdayCreate( dayNum=4, dayName="Thursday" )
        dday4 = Weekday.model_validate( day4 )
        day5 = WeekdayCreate( dayNum=5, dayName="Friday" )
        dday5 = Weekday.model_validate( day5 )
        day6 = WeekdayCreate( dayNum=6, dayName="Saturday" )
        dday6 = Weekday.model_validate( day6 )
        day7 = WeekdayCreate( dayNum=7, dayName="Sunday" )
        dday7 = Weekday.model_validate( day7 )
        session.add( dday1 )
        session.add( dday2 )
        session.add( dday3 )
        session.add( dday4 )
        session.add( dday5 )
        session.add( dday6 )
        session.add( dday7 )
        session.commit()
        return {"ok": True}

def createSubjects( session: Session ):
        sbj1 = SubjectCreate( Name="Matematika" )
        dsbj1 = Subject.model_validate( sbj1 )
        sbj2 = SubjectCreate( Name="Russky" )
        dsbj2 = Subject.model_validate( sbj2 )
        sbj3 = SubjectCreate( Name="Geography" )
        dsbj3 = Subject.model_validate( sbj3 )
        sbj4 = SubjectCreate( Name="Music" )
        dsbj4 = Subject.model_validate( sbj4 )
        sbj5 = SubjectCreate( Name="Trud" )
        dsbj5 = Subject.model_validate( sbj5 )
        session.add( dsbj1 )
        session.add( dsbj2 )
        session.add( dsbj3 )
        session.add( dsbj4 )
        session.add( dsbj5 )
        session.commit()
        return {"ok": True}