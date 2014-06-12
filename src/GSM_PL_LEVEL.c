#include "..\inc\GSM_PL_LEVEL.h"
#include "..\inc\GSM_AL_LEVEL.h"
#include "..\inc\ATCOMM_HAL.h"
#include <string.h>
#include <stdio.h>

char buf[256] = "";                                                     // temporary buffer for operations with ring buffers

void run_gsm_queue(gsm_modem* modem) {
    uint8_t i = 0;                                                          // cycle variable

    switch(modem->state) {                                                  // Modem state cases:
    case MODEM_IDLE:                                                        // scenario is finished, do nothing
        break;
    case MODEM_SKIP_CMD:
        #ifdef __DEBUG__
        printf("\r\nSkipping %d action!\r\n",modem->cur_action);
        #endif // __DEBUG__
        while (*(modem->action_queue.base + modem->action_queue.tail++) != EOSchar) ;
        //modem->action_queue.tail++;
        modem->cur_action++;
        modem->state = MODEM_CMD_SEND;
        break;
    case MODEM_CMD_SEND:                                                    // need to send command
            //printf("Command #%d: \r\n",modem->cur_action);                  // DEBUG!
            // extract command from queue
            if ( modem->action_queue.head != modem->action_queue.tail ) {
//                for(i = 0;(*(modem->action_queue.base+modem->action_queue.tail) != EOSchar); i++)
//                    buf[i] = *(modem->action_queue.base+modem->action_queue.tail++);
                i=0; while (*(modem->action_queue.base+modem->action_queue.tail) != EOSchar)
                        buf[i++] = *(modem->action_queue.base+modem->action_queue.tail++);

                buf[i++] = EOSchar;
                modem->action_queue.tail++;                                 // to avoid \0 catch
                                                                            // send cmd
                modem->send_cmd(buf);
                #ifdef __DEBUG__
                printf(">> %s \r\n",buf);
                #endif // __DEBUG__
                modem->state = MODEM_ANS_WAIT;                              // wait for answer
            }
        break;

    case MODEM_ANS_WAIT:                                                    // waiting for answer, clears by uart interrupt
        break;

    case MODEM_ANS_RECV:                                                    // parsing answer
        // extract command from queue
        if ( modem->answers.head != modem->answers.tail ) {
          if (*(modem->answers.base + (modem->answers.head-1)) == '\n') {

//            while (modem->answers.head != modem->answers.tail)
//                buf[i++] = *(modem->answers.base + modem->answers.tail++);
//            buf[i++] = EOSchar;
            i=0;
            while (*(modem->answers.base + (modem->answers.tail)) != '\n')
                buf[i++] = *(modem->answers.base + modem->answers.tail++);
            modem->answers.tail++;buf[i++] = '\n';buf[i++] = EOSchar;

            // callback answer parsing
            switch(modem->callback(buf,modem->cur_action)) {
            case 0:                                                         // still must receive answer
                // take a rest, bro
                //for(i = 0; i < 255; i++) buf[i] = 0x00;// dummy erasing for possible answer echoing
                break;
            case 1:                                                         // receiving the answer is finished, go to next cmd
                modem->cur_action++;
                if (modem->action_queue.head == modem->action_queue.tail)   // if commands no more - set modem to idle state
                    modem->state = MODEM_IDLE;
                else
                    modem->state = MODEM_CMD_SEND;                          // else send next cmd
                break;
            case 2:
                #ifdef __DEBUG__
                printf("Error. Whoopsie! (c) Catbug \r\n");
                #endif
                break;
            case 3: // SKIP COMMAND

                modem->cur_action++;
                if (modem->action_queue.head == modem->action_queue.tail)   // if commands no more - set modem to idle state
                    modem->state = MODEM_IDLE;
                else
                    modem->state = MODEM_SKIP_CMD;                          // else send next cmd

                break;
            default:
                #ifdef __DEBUG__
                printf("Default answer handler catch! \r\n");
                #endif // __DEBUG__
                break;
            }   // switch(modem->callback())
          }   //if (*(modem->answers.base + (modem->answers.head-1)) == '\n')
        }       // if( modem->answers.head != modem->answers.tail )
        break;
    }           // switch(modem->state)
    return;
}

void gsm_add_task(gsm_modem* modem, gsm_scenario* scenario){
    #if SCENARIO_MAX_LEN < 255
    uint8_t i = 0;
    #else
    uint16_t i = 0;
    #endif // SCENARIO_MAX_LEN
    uint8_t j = 0;
    char _cmd[15] = "";
    /* matching AL commands with AT commands */
    while(scenario->actions[i].action_type != SCEN_FINISH) {  // goes over all actions
        switch(scenario->actions[i].cmd) {
        case AC_PRESENSE:
            strcpy(_cmd, AT_PRESENSE);
            break;
        case AC_ECHOOFF:
            strcpy(_cmd, AT_ECHOOFF);
            break;
        case AC_ECHOON:
            strcpy(_cmd, AT_ECHOON);
            break;
        case AC_GET_VENDOR:
            strcpy(_cmd, AT_GET_VENDOR);
            break;
        case AC_GET_MODEL:
            strcpy(_cmd, AT_GET_MODEL);
            break;
        case AC_USSD:
            strcpy(_cmd, AT_USSD);
            break;
        case AC_PINCODE:
            strcpy(_cmd, AT_PINCODE);
            break;
//        case AC_PUKCODE:
//            strcpy(_cmd, AT_PUKCODE);
            break;
        }
        /* copying command in buffer */
        for (j = 0; _cmd[j] != '\0' ;j++) {
            *(modem->action_queue.base + modem->action_queue.head) = *(_cmd+j);
            modem->action_queue.head++;
        }
        /* looking for end of command */
        switch(scenario->actions[i].action_type) {
        case EXEC_CMD:
            ring_push(modem->action_queue,'\r');
            break;
        case TEST_CMD:
            ring_push(modem->action_queue,'=');
            ring_push(modem->action_queue,'?');
            ring_push(modem->action_queue,'\r');    // if test commands type - add "=?\r"
            break;
        case READ_CMD:
            ring_push(modem->action_queue,'?');
            ring_push(modem->action_queue,'\r');
            break;
        case WRITE_CMD:
            ring_push(modem->action_queue,'=');
            while(*(scenario->actions[i].pParams)) {
                *(modem->action_queue.base + modem->action_queue.head++) = *(scenario->actions[i].pParams++);
            }
            ring_push(modem->action_queue,'\r');
            break;
        case SCEN_FINISH:
            break;
        default:
            break;
        }
        ring_push(modem->action_queue,EOSchar);
        i++;
    }
    modem->callback = scenario->callback;
    modem->cur_action = MODEM_CMD_SEND;
    return;
}

void    gsm_queue_halt(gsm_modem* modem){
    /* states terminating */
    modem->state        = MODEM_IDLE;
    modem->cur_action   = 0;
    /* sending ring buffer terminating */
    modem->action_queue.head = 0;
    modem->action_queue.tail = 0;
    /* receiving ring buffer terminating */
    modem->answers.head = 0;
    modem->answers.tail = 0;

    return;
}
