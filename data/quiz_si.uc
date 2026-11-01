#game "serpentisle"
// Built-in quiz slider correction. Mods retain normal override authority.
// Only the initial numeric selection differs from the original helper.
// Shared by Fedabiblio and the Gwanni child; keep the original retry/flag logic.
var askCopyProtection 0x98C () {
    var v0, v1, v2, v3, v4, v5;

    v0 = false;
    while (true) {
        v1 = false;
        v2 = 0;
        while (v2 < 3) {
            v2 = (v2 + 1);
            v3 = false;
            while (!v3) {
                v4 = UI_die_roll(1, 15);
                if ((v4 == 1) && !gflags[26]) {
                    gflags[26] = true;
                    v3 = true;
                    say("\"According to Erstam's book, how many spells are available in the Ninth Circle of Magic?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 8);
                    if (v5 != 8) {
                        v1 = true;
                    }
                }
                if ((v4 == 2) && !gflags[27]) {
                    gflags[27] = true;
                    v3 = true;
                    say("\"How many different materials can armour be made from?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 4);
                    if (v5 != 4) {
                        v1 = true;
                    }
                }
                if ((v4 == 3) && !gflags[28]) {
                    gflags[28] = true;
                    v3 = true;
                    say("\"How many beasts are discussed in the Bestiary in Erstam's book?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 52);
                    if (v5 != 52) {
                        v1 = true;
                    }
                }
                if ((v4 == 4) && !gflags[29]) {
                    gflags[29] = true;
                    v3 = true;
                    say("\"How many different types of Reagents doth Erstam write about?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 11);
                    if (v5 != 11) {
                        v1 = true;
                    }
                }
                if ((v4 == 5) && !gflags[30]) {
                    gflags[30] = true;
                    v3 = true;
                    say("\"How many reagents are required to cast the @Mass Death@ spell?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 6);
                    if (v5 != 6) {
                        v1 = true;
                    }
                }
                if ((v4 == 6) && !gflags[31]) {
                    gflags[31] = true;
                    v3 = true;
                    say("\"How many times must ginseng be boiled before it becomes syrup?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 40);
                    if (v5 != 40) {
                        v1 = true;
                    }
                }
                if ((v4 == 7) && !gflags[32]) {
                    gflags[32] = true;
                    v3 = true;
                    say("\"In what Circle of Magic doth the Create Automata spell appear?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 6);
                    if (v5 != 6) {
                        v1 = true;
                    }
                }
                if ((v4 == 8) && !gflags[33]) {
                    gflags[33] = true;
                    v3 = true;
                    say("\"For how many reasons did Erstam put down the history of the land in writing?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 1);
                    if (v5 != 1) {
                        v1 = true;
                    }
                }
                if ((v4 == 9) && !gflags[34]) {
                    gflags[34] = true;
                    v3 = true;
                    say("\"How many characters are there in Ophidian writing?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 36);
                    if (v5 != 36) {
                        v1 = true;
                    }
                }
                if ((v4 == 10) && !gflags[35]) {
                    gflags[35] = true;
                    v3 = true;
                    say("\"In Erstam's book, the section entitled @Other Landmarks@ covers how many areas?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 4);
                    if (v5 != 4) {
                        v1 = true;
                    }
                }
                if ((v4 == 11) && !gflags[36]) {
                    gflags[36] = true;
                    v3 = true;
                    say("\"How many cities were established in the Serpent Isle?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 3);
                    if (v5 != 3) {
                        v1 = true;
                    }
                }
                if ((v4 == 12) && !gflags[37]) {
                    gflags[37] = true;
                    v3 = true;
                    say("\"How many forces, when combined, form the Principles of Balance?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 6);
                    if (v5 != 6) {
                        v1 = true;
                    }
                }
                if ((v4 == 13) && !gflags[38]) {
                    gflags[38] = true;
                    v3 = true;
                    say("\"How many clans divide the city of Monitor?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 3);
                    if (v5 != 3) {
                        v1 = true;
                    }
                }
                if ((v4 == 14) && !gflags[39]) {
                    gflags[39] = true;
                    v3 = true;
                    say("\"How many types of weapons are discussed in Erstam's book?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 4);
                    if (v5 != 4) {
                        v1 = true;
                    }
                }
                if ((v4 == 15) && !gflags[40]) {
                    gflags[40] = true;
                    v3 = true;
                    say("\"How many Words of Power are there?\"");
                    v5 = UI_input_numeric_value(1, 70, 1, 27);
                    if (v5 != 27) {
                        v1 = true;
                    }
                }
            }
        }
        if (v0 && v1) {
            UI_set_item_flag(-356, 25);
            return false;
        }
        if (v1) {
            say("\"I am not yet convinced...\"");
            v0 = true;
            continue;
        } else {
            return true;
        }
    }
}
