// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.schedule.Day;
import com.dxfeed.schedule.DayFilter;
import com.dxfeed.schedule.Session;
import com.dxfeed.schedule.SessionFilter;
import com.dxfeed.schedule.SessionType;

/**
 * Internal caller: a Java 8 fixture capture invokes observe with an actual production Day. The
 * returned tab-separated fields project public filter/query results; source exceptions other than
 * no-match propagate.
 */
public final class FilterObservation {
  private FilterObservation() {}

  private static String session(Session value) {
    return value == null
        ? "NONE"
        : value.getDay().getDayId()
            + ":"
            + value.getType().name()
            + ":"
            + value.getStartTime()
            + ":"
            + value.getEndTime()
            + ":"
            + (value.isTrading() ? 1 : 0)
            + ":"
            + (value.isEmpty() ? 1 : 0);
  }

  private static String day(Day value) {
    return value == null ? "NONE" : Integer.toString(value.getYearMonthDay());
  }

  public static String observe(Day day) {
    DayFilter neverDay = new DayFilter(1, null, null, null);
    SessionFilter neverSession = new SessionFilter(SessionType.REGULAR, false);
    Session first = day.getFirstSession(SessionFilter.ANY);
    Session last = day.getLastSession(SessionFilter.ANY);
    String dayError;
    String sessionError;
    try {
      day.getNextDay(neverDay);
      dayError = "UNEXPECTED_MATCH";
    } catch (java.util.NoSuchElementException e) {
      dayError = "ERROR:NoSuchElementException";
    }
    try {
      day.getFirstSession(neverSession);
      sessionError = "UNEXPECTED_MATCH";
    } catch (java.util.NoSuchElementException e) {
      sessionError = "ERROR:NoSuchElementException";
    }
    return (DayFilter.TRADING.accept(day) ? 1 : 0)
        + ","
        + (DayFilter.HOLIDAY.accept(day) ? 1 : 0)
        + ","
        + (DayFilter.SHORT_DAY.accept(day) ? 1 : 0)
        + ","
        + (DayFilter.WEEK_DAY.accept(day) ? 1 : 0)
        + "\t"
        + day(day.findPrevDay(DayFilter.TRADING))
        + "\t"
        + day(day.findNextDay(DayFilter.TRADING))
        + "\t"
        + day(day.findNextDay(neverDay))
        + "\t"
        + dayError
        + "\t"
        + session(day.findFirstSession(SessionFilter.TRADING))
        + "\t"
        + session(day.findLastSession(SessionFilter.TRADING))
        + "\t"
        + session(day.findFirstSession(neverSession))
        + "\t"
        + sessionError
        + "\t"
        + session(first.findNextSession(SessionFilter.REGULAR))
        + "\t"
        + session(last.findPrevSession(SessionFilter.REGULAR))
        + "\t"
        + session(last.findNextSession(neverSession));
  }
}
