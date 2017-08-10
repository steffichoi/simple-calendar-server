A simple calendar server that supports simple operations.
After compiling calserver and calendar, the service is accessible through
the commandline through a few commands.

These commands include:

>add_calendar calendar-name

-adds a new calendar to the list of calendars

>list_calendars
-outputs a list of calendars

>add_user user-name
-adds a new user to the list of users

>list_users

-outputs a list of users

>subscribe user-name calendar-name

-subscribes the user(use-name) to the calendar(calendar-name)

-will give an error if the user is already suscribed to the calendar

>add_event calendar-name event-name time

-adds an event(event-name) to the calendar(calendar-name) at the specified
time

>list_events calendar-name event-name

-outputs the list of events associated with the calendar(calendar-name)
in the format  

		event-name:Day Month Date hh:mm:ss year

sorted by time


This simple server was built in C, utilizing basic structs and simple methods
to store and retrieve data stored in the structs.

#####STEPS TO RUN THE PROGRAM
call the makefile
ensure that all essential programs have been built, specifically:
calserver and calendar

open two terminals
in one, start the server by running
>./calserver 

in the other, start the calendar service by running
>./calendar

now that both programs are running, you can use the commands listed above
