/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2022 Mladen Ivkovic (mladen.ivkovic@hotmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#ifndef SWIFT_RT_RESCHEDULE_H
#define SWIFT_RT_RESCHEDULE_H

/**
 * @file src/rt_reschedule.h
 * @brief Main header file for the rescheduling of RT tasks
 */

/* TODO: temporary for dev */
#define RT_RESCHEDULE_MAX 10

void rt_reschedule_task(struct engine* e, struct task *t, struct cell* c, int wait, int callloc);
void rt_reschedule_rescheduler(struct runner *r, struct cell *c, struct task *rescheduler_task);
int rt_reschedule(struct runner *r, struct cell *c);

#endif /* defined SWIFT_RT_RESCHEDULE_H */
