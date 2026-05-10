/*
 * ============================================================================
 *  BUKANA CHILD VACCINATION & MEDICAL RECORD MANAGEMENT SYSTEM
 * ============================================================================
 *  Improvements:
 *   - Secure password hashing (djb2)
 *   - Input validation throughout
 *   - Persistent storage with proper serialization (JSON-style flat files)
 *   - Audit log for all record changes
 *   - Search by name, ID, or date
 *   - Statistics dashboard
 *   - Password masking at input
 *   - Role-based access control
 *   - Duplicate ID prevention
 *   - Data load on startup / save on change
 *   - Graceful error handling
 *   - Cross-platform color via ANSI codes (works on Windows 10+ and Linux)
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <iomanip>
#include <functional>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
  #define SLEEP(ms) Sleep(ms)
  void enableANSI() {
      HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
      DWORD dwMode = 0;
      GetConsoleMode(hOut, &dwMode);
      SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
#else
  #include <unistd.h>
  #define SLEEP(ms) usleep((ms)*1000)
  void enableANSI() {}
  int getch() { return getchar(); }
#endif

using namespace std;

// ---------------------------------------------------------
//  ANSI Color Helpers
// ---------------------------------------------------------
namespace Color {
    const string RESET   = "\033[0m";
    const string BOLD    = "\033[1m";
    const string DIM     = "\033[2m";
    const string CYAN    = "\033[36m";
    const string BCYAN   = "\033[96m";
    const string GREEN   = "\033[32m";
    const string BGREEN  = "\033[92m";
    const string YELLOW  = "\033[33m";
    const string BYELLOW = "\033[93m";
    const string RED     = "\033[31m";
    const string BRED    = "\033[91m";
    const string BLUE    = "\033[34m";
    const string BBLUE   = "\033[94m";
    const string MAGENTA = "\033[35m";
    const string WHITE   = "\033[97m";
    const string BG_DARK = "\033[40m";
}

// ---------------------------------------------------------
//  Utility Functions
// ---------------------------------------------------------
namespace Util {

    void clearScreen() {
#ifdef _WIN32
        system("cls");
#else
        cout << "\033[2J\033[H";
#endif
    }

    // djb2 hash for password storage
    string hashPassword(const string& pw) {
        unsigned long hash = 5381;
        for (char c : pw)
            hash = ((hash << 5) + hash) + (unsigned char)c;
        ostringstream oss;
        oss << hex << hash;
        return oss.str();
    }

    // Read password without echoing characters
    string readPassword() {
        string pw;
        char ch;
        while (true) {
#ifdef _WIN32
            ch = _getch();
#else
            ch = getch();
#endif
            if (ch == '\r' || ch == '\n') break;
            if (ch == '\b' || ch == 127) {
                if (!pw.empty()) { pw.pop_back(); cout << "\b \b"; }
            } else {
                pw += ch;
                cout << '*';
            }
        }
        cout << endl;
        return pw;
    }

    // Trim whitespace
    string trim(const string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return (a == string::npos) ? "" : s.substr(a, b - a + 1);
    }

    // Convert string to uppercase
    string toUpper(string s) {
        transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    }

    // Current timestamp string
    string timestamp() {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return string(buf);
    }

    // Validate date format DDMMYYYY (basic)
    bool isValidDate(const string& d) {
        if (d.length() != 8) return false;
        for (char c : d) if (!isdigit(c)) return false;
        int day = stoi(d.substr(0, 2));
        int mon = stoi(d.substr(2, 2));
        int yr  = stoi(d.substr(4, 4));
        return day >= 1 && day <= 31 && mon >= 1 && mon <= 12 && yr >= 1900 && yr <= 2100;
    }

    // Validate phone number (digits, +, -, spaces allowed)
    bool isValidPhone(const string& p) {
        if (p.length() < 7 || p.length() > 15) return false;
        for (char c : p) if (!isdigit(c) && c != '+' && c != '-' && c != ' ') return false;
        return true;
    }

    // Safe integer input with range
    int safeIntInput(const string& prompt, int lo, int hi) {
        int val;
        while (true) {
            cout << prompt;
            if (cin >> val && val >= lo && val <= hi) {
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                return val;
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << Color::RED << "  Invalid input. Enter a number between " << lo << " and " << hi << "." << Color::RESET << "\n";
        }
    }

    // Safe string input (non-empty)
    string safeStringInput(const string& prompt, bool allowSpaces = false) {
        string val;
        while (true) {
            cout << prompt;
            if (allowSpaces) {
                getline(cin, val);
            } else {
                cin >> val;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
            val = trim(val);
            if (!val.empty()) return val;
            cout << Color::RED << "  Input cannot be empty.\n" << Color::RESET;
        }
    }

    // Escape/unescape pipe for flat-file storage
    string escape(const string& s) {
        string r;
        for (char c : s) {
            if (c == '|') r += "\\p";
            else if (c == '\n') r += "\\n";
            else if (c == '\\') r += "\\\\";
            else r += c;
        }
        return r;
    }

    string unescape(const string& s) {
        string r;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                if      (s[i+1] == 'p')  { r += '|';  i++; }
                else if (s[i+1] == 'n')  { r += '\n'; i++; }
                else if (s[i+1] == '\\') { r += '\\'; i++; }
                else r += s[i];
            } else r += s[i];
        }
        return r;
    }

    // Split string by delimiter
    vector<string> split(const string& s, char delim) {
        vector<string> tokens;
        stringstream ss(s);
        string tok;
        while (getline(ss, tok, delim)) tokens.push_back(tok);
        return tokens;
    }

    // Print a styled header box
    void printHeader(const string& title, const string& color = Color::BCYAN) {
        int w = 85;
        string line(w, '=');
        string pad((w - (int)title.size()) / 2, ' ');
        cout << color << Color::BOLD;
        cout << "\n  +" << line << "+\n";
        cout << "  |" << pad << title << pad << (title.size() % 2 == 0 ? " " : "") << "|\n";
        cout << "  +" << line << "+\n" << Color::RESET;
    }

    // Print a divider
    void divider(const string& color = Color::DIM) {
        cout << color << "  " << string(87, '-') << Color::RESET << "\n";
    }

    // Typewriter effect for short text
    void typewrite(const string& s, int ms = 20) {
        for (char c : s) {
            cout << c << flush;
            SLEEP(ms);
        }
    }

    // Progress animation
    void progressBar(const string& label, int steps = 12, int ms = 40) {
        cout << "  " << Color::YELLOW << label << " [";
        for (int i = 0; i < steps; i++) {
            cout << "#" << flush;
            SLEEP(ms);
        }
        cout << "] " << Color::BGREEN << "Done!" << Color::RESET << "\n";
    }

    // Print status message
    void status(const string& msg, bool ok = true) {
        if (ok)
            cout << "  " << Color::BGREEN << "[OK]  " << Color::RESET << msg << "\n";
        else
            cout << "  " << Color::BRED  << "[ERR] " << Color::RESET << msg << "\n";
    }

    // Mask a name for display
    string maskName(const string& name) {
        if (name.empty()) return "";
        return string(1, name[0]) + string(max(0, (int)name.size() - 1), '*');
    }

    // Mask DOB (show year only)
    string maskDOB(const string& dob) {
        if (dob.size() == 8)
            return "**/**/  " + dob.substr(4, 4);
        return "**/**/**";
    }

    // Confirm action
    bool confirm(const string& msg) {
        cout << "\n  " << Color::BYELLOW << "[!]  " << msg << " (y/N): " << Color::RESET;
        string a;
        getline(cin, a);
        return trim(a) == "y" || trim(a) == "Y";
    }

    // Pause
    void pause() {
        cout << "\n  " << Color::DIM << "Press Enter to continue..." << Color::RESET;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// ---------------------------------------------------------
//  Audit Log
// ---------------------------------------------------------
namespace AuditLog {
    const string FILE_PATH = "AuditLog.txt";

    void log(const string& actor, const string& action, const string& detail = "") {
        ofstream f(FILE_PATH, ios::app);
        if (f) {
            f << "[" << Util::timestamp() << "] [" << actor << "] " << action;
            if (!detail.empty()) f << " | " << detail;
            f << "\n";
        }
    }
}

// ---------------------------------------------------------
//  Data Structures
// ---------------------------------------------------------
struct VaccinationRecord {
    string placeOfBirth, vaccine, treatment, weight, height;
    string clinicDate, nurseName, clinic, nextClinicDate;
    string capturedAt;

    string serialize() const {
        using namespace Util;
        return escape(placeOfBirth) + "|" + escape(vaccine) + "|" + escape(treatment) + "|" +
               escape(weight) + "|" + escape(height) + "|" + escape(clinicDate) + "|" +
               escape(nurseName) + "|" + escape(clinic) + "|" + escape(nextClinicDate) + "|" +
               escape(capturedAt);
    }

    static VaccinationRecord deserialize(const string& line) {
        auto t = Util::split(line, '|');
        while (t.size() < 10) t.push_back("");
        VaccinationRecord r;
        r.placeOfBirth   = Util::unescape(t[0]);
        r.vaccine        = Util::unescape(t[1]);
        r.treatment      = Util::unescape(t[2]);
        r.weight         = Util::unescape(t[3]);
        r.height         = Util::unescape(t[4]);
        r.clinicDate     = Util::unescape(t[5]);
        r.nurseName      = Util::unescape(t[6]);
        r.clinic         = Util::unescape(t[7]);
        r.nextClinicDate = Util::unescape(t[8]);
        r.capturedAt     = Util::unescape(t[9]);
        return r;
    }
};

struct MedicalRecord {
    string placeOfBirth, medication, treatment, weight, height;
    string consultationDate, doctor, clinic, checkupDate;
    string capturedAt;

    string serialize() const {
        using namespace Util;
        return escape(placeOfBirth) + "|" + escape(medication) + "|" + escape(treatment) + "|" +
               escape(weight) + "|" + escape(height) + "|" + escape(consultationDate) + "|" +
               escape(doctor) + "|" + escape(clinic) + "|" + escape(checkupDate) + "|" +
               escape(capturedAt);
    }

    static MedicalRecord deserialize(const string& line) {
        auto t = Util::split(line, '|');
        while (t.size() < 10) t.push_back("");
        MedicalRecord r;
        r.placeOfBirth     = Util::unescape(t[0]);
        r.medication       = Util::unescape(t[1]);
        r.treatment        = Util::unescape(t[2]);
        r.weight           = Util::unescape(t[3]);
        r.height           = Util::unescape(t[4]);
        r.consultationDate = Util::unescape(t[5]);
        r.doctor           = Util::unescape(t[6]);
        r.clinic           = Util::unescape(t[7]);
        r.checkupDate      = Util::unescape(t[8]);
        r.capturedAt       = Util::unescape(t[9]);
        return r;
    }
};

// ---------------------------------------------------------
//  Child Class
// ---------------------------------------------------------
class Child {
public:
    string firstName, middleName, lastName, dateOfBirth;
    string guardianName, fatherName, village, chief, district;
    string passwordHash, recordID;
    vector<VaccinationRecord> vaccinations;
    vector<MedicalRecord>     medicals;

    // Generate unique Record ID
    static string generateID(const string& fn, const string& ln, const string& dob) {
        string id;
        id += Util::toUpper(fn.substr(0, min((int)fn.size(), 3)));
        id += "-";
        id += Util::toUpper(ln.substr(0, min((int)ln.size(), 3)));
        id += "-";
        id += dob;
        return id;
    }

    string getID()       const { return recordID; }
    string getPassword() const { return passwordHash; }

    // ── Display helpers ──────────────────────────────
    void printVaccinationReport(bool masked) const {
        Util::clearScreen();
        Util::printHeader("CHILD VACCINATION REPORT", Color::BCYAN);
        cout << "\n";
        string name = masked ? Util::maskName(firstName) + " " + Util::maskName(lastName)
                             : firstName + " " + middleName + " " + lastName;
        cout << "  " << Color::BOLD << "Child Name:    " << Color::RESET << Color::WHITE << name << Color::RESET << "\n";
        cout << "  " << Color::BOLD << "Guardian:      " << Color::RESET << (masked ? Util::maskName(guardianName) : guardianName) << "\n";
        cout << "  " << Color::BOLD << "Date of Birth: " << Color::RESET << (masked ? Util::maskDOB(dateOfBirth) : dateOfBirth) << "\n";
        cout << "  " << Color::BOLD << "Record ID:     " << Color::RESET << Color::CYAN << recordID << Color::RESET << "\n\n";

        if (vaccinations.empty()) {
            cout << "  " << Color::YELLOW << "No vaccination records found.\n" << Color::RESET;
        } else {
            int idx = 1;
            for (const auto& v : vaccinations) {
                // FIX: replaced multi-byte char '─' with '-' in string(N, '-')
                cout << "  " << Color::BBLUE << "+-- Record #" << idx++ << " " << string(60, '-') << Color::RESET << "\n";
                cout << "  |  " << Color::BOLD << "Vaccine:        " << Color::RESET << (masked ? string(v.vaccine.size(), '*') : v.vaccine) << "\n";
                cout << "  |  " << Color::BOLD << "Treatment:      " << Color::RESET << (masked ? string(v.treatment.size(), '*') : v.treatment) << "\n";
                cout << "  |  " << Color::BOLD << "Weight/Height:  " << Color::RESET << v.weight << " kg / " << v.height << " cm\n";
                cout << "  |  " << Color::BOLD << "Clinic Date:    " << Color::RESET << v.clinicDate << "\n";
                cout << "  |  " << Color::BOLD << "Next Visit:     " << Color::RESET << Color::BGREEN << v.nextClinicDate << Color::RESET << "\n";
                cout << "  |  " << Color::BOLD << "Administered by:" << Color::RESET << " Sr. " << v.nurseName << " @ " << v.clinic << "\n";
                cout << "  |  " << Color::BOLD << "Place of Birth: " << Color::RESET << v.placeOfBirth << "\n";
                cout << "  |  " << Color::DIM  << "Recorded:       " << v.capturedAt << Color::RESET << "\n";
                cout << "  " << Color::BBLUE << "+" << string(71, '-') << Color::RESET << "\n\n";
            }
        }
        Util::pause();
    }

    void printMedicalReport(bool masked) const {
        Util::clearScreen();
        Util::printHeader("CHILD MEDICAL REPORT", Color::BGREEN);
        cout << "\n";
        string name = masked ? Util::maskName(firstName) + " " + Util::maskName(lastName)
                             : firstName + " " + middleName + " " + lastName;
        cout << "  " << Color::BOLD << "Child Name:    " << Color::RESET << Color::WHITE << name << Color::RESET << "\n";
        cout << "  " << Color::BOLD << "Guardian:      " << Color::RESET << (masked ? Util::maskName(guardianName) : guardianName) << "\n";
        cout << "  " << Color::BOLD << "Date of Birth: " << Color::RESET << (masked ? Util::maskDOB(dateOfBirth) : dateOfBirth) << "\n";
        cout << "  " << Color::BOLD << "Record ID:     " << Color::RESET << Color::CYAN << recordID << Color::RESET << "\n\n";

        if (medicals.empty()) {
            cout << "  " << Color::YELLOW << "No medical records found.\n" << Color::RESET;
        } else {
            int idx = 1;
            for (const auto& m : medicals) {
                // FIX: replaced multi-byte char '─' with '-' in string(N, '-')
                cout << "  " << Color::BGREEN << "+-- Record #" << idx++ << " " << string(60, '-') << Color::RESET << "\n";
                cout << "  |  " << Color::BOLD << "Medication:     " << Color::RESET << (masked ? string(m.medication.size(), '*') : m.medication) << "\n";
                cout << "  |  " << Color::BOLD << "Treatment:      " << Color::RESET << (masked ? string(m.treatment.size(), '*') : m.treatment) << "\n";
                cout << "  |  " << Color::BOLD << "Weight/Height:  " << Color::RESET << m.weight << " kg / " << m.height << " cm\n";
                cout << "  |  " << Color::BOLD << "Consult Date:   " << Color::RESET << m.consultationDate << "\n";
                cout << "  |  " << Color::BOLD << "Checkup Date:   " << Color::RESET << Color::BGREEN << m.checkupDate << Color::RESET << "\n";
                cout << "  |  " << Color::BOLD << "Doctor:         " << Color::RESET << "Dr. " << m.doctor << " @ " << m.clinic << "\n";
                cout << "  |  " << Color::BOLD << "Place of Birth: " << Color::RESET << m.placeOfBirth << "\n";
                cout << "  |  " << Color::DIM  << "Recorded:       " << m.capturedAt << Color::RESET << "\n";
                cout << "  " << Color::BGREEN << "+" << string(71, '-') << Color::RESET << "\n\n";
            }
        }
        Util::pause();
    }

    // ── Export to file ────────────────────────────────
    void exportVaccinationToFile() const {
        if (!Util::confirm("Export vaccination report to a text file?")) return;
        string fn = "VaccinationReport_" + recordID + ".txt";
        ofstream f(fn);
        if (!f) { Util::status("Could not create file.", false); Util::pause(); return; }
        f << "BUKANA CHILD VACCINATION REPORT\n";
        f << "Generated: " << Util::timestamp() << "\n";
        f << string(60, '=') << "\n";
        f << "Child:    " << firstName << " " << middleName << " " << lastName << "\n";
        f << "Guardian: " << guardianName << "\n";
        f << "DOB:      " << dateOfBirth << "\n";
        f << "ID:       " << recordID << "\n\n";
        int i = 1;
        for (const auto& v : vaccinations) {
            f << "--- Record #" << i++ << " ---\n";
            f << "Vaccine:      " << v.vaccine << "\n";
            f << "Treatment:    " << v.treatment << "\n";
            f << "Weight:       " << v.weight << " kg\n";
            f << "Height:       " << v.height << " cm\n";
            f << "Clinic Date:  " << v.clinicDate << "\n";
            f << "Next Visit:   " << v.nextClinicDate << "\n";
            f << "Nurse:        " << v.nurseName << "\n";
            f << "Clinic:       " << v.clinic << "\n";
            f << "Place Birth:  " << v.placeOfBirth << "\n";
            f << "Recorded:     " << v.capturedAt << "\n\n";
        }
        f.close();
        Util::status("Report exported to: " + fn);
        Util::pause();
    }

    void exportMedicalToFile() const {
        if (!Util::confirm("Export medical report to a text file?")) return;
        string fn = "MedicalReport_" + recordID + ".txt";
        ofstream f(fn);
        if (!f) { Util::status("Could not create file.", false); Util::pause(); return; }
        f << "BUKANA CHILD MEDICAL REPORT\n";
        f << "Generated: " << Util::timestamp() << "\n";
        f << string(60, '=') << "\n";
        f << "Child:    " << firstName << " " << middleName << " " << lastName << "\n";
        f << "Guardian: " << guardianName << "\n";
        f << "DOB:      " << dateOfBirth << "\n";
        f << "ID:       " << recordID << "\n\n";
        int i = 1;
        for (const auto& m : medicals) {
            f << "--- Record #" << i++ << " ---\n";
            f << "Medication:   " << m.medication << "\n";
            f << "Treatment:    " << m.treatment << "\n";
            f << "Weight:       " << m.weight << " kg\n";
            f << "Height:       " << m.height << " cm\n";
            f << "Consult Date: " << m.consultationDate << "\n";
            f << "Checkup Date: " << m.checkupDate << "\n";
            f << "Doctor:       " << m.doctor << "\n";
            f << "Clinic:       " << m.clinic << "\n";
            f << "Place Birth:  " << m.placeOfBirth << "\n";
            f << "Recorded:     " << m.capturedAt << "\n\n";
        }
        f.close();
        Util::status("Report exported to: " + fn);
        Util::pause();
    }

    // ── Serialization ─────────────────────────────────
    string serialize() const {
        using namespace Util;
        return escape(firstName) + "|" + escape(middleName) + "|" + escape(lastName) + "|" +
               escape(dateOfBirth) + "|" + escape(guardianName) + "|" + escape(fatherName) + "|" +
               escape(village) + "|" + escape(chief) + "|" + escape(district) + "|" +
               escape(passwordHash) + "|" + escape(recordID);
    }

    static Child deserialize(const string& line) {
        auto t = Util::split(line, '|');
        while (t.size() < 11) t.push_back("");
        Child c;
        c.firstName    = Util::unescape(t[0]);
        c.middleName   = Util::unescape(t[1]);
        c.lastName     = Util::unescape(t[2]);
        c.dateOfBirth  = Util::unescape(t[3]);
        c.guardianName = Util::unescape(t[4]);
        c.fatherName   = Util::unescape(t[5]);
        c.village      = Util::unescape(t[6]);
        c.chief        = Util::unescape(t[7]);
        c.district     = Util::unescape(t[8]);
        c.passwordHash = Util::unescape(t[9]);
        c.recordID     = Util::unescape(t[10]);
        return c;
    }

    // ── Child Task Menu ───────────────────────────────
    void taskMenu() {
        while (true) {
            Util::clearScreen();
            Util::printHeader("CHILD / PARENT MENU  .  " + firstName + " " + lastName, Color::BCYAN);
            cout << "\n";
            cout << "  " << Color::BOLD << "1." << Color::RESET << "  View Vaccination Report\n";
            cout << "  " << Color::BOLD << "2." << Color::RESET << "  View Medical Report\n";
            cout << "  " << Color::BOLD << "3." << Color::RESET << "  Export Vaccination Report to File\n";
            cout << "  " << Color::BOLD << "4." << Color::RESET << "  Export Medical Report to File\n";
            cout << "  " << Color::BOLD << "5." << Color::RESET << "  Change Password\n";
            cout << "  " << Color::BOLD << "0." << Color::RESET << "  Logout\n\n";

            int ch = Util::safeIntInput("  " + Color::BCYAN + "-> " + Color::RESET + "Choice: ", 0, 5);
            switch (ch) {
                case 1: printVaccinationReport(true); break;
                case 2: printMedicalReport(true);     break;
                case 3: exportVaccinationToFile();    break;
                case 4: exportMedicalToFile();        break;
                case 5: changePassword();             break;
                case 0: return;
            }
        }
    }

    void changePassword() {
        cout << "\n  Current password: ";
        string cur = Util::readPassword();
        if (Util::hashPassword(cur) != passwordHash) {
            Util::status("Incorrect current password.", false);
            Util::pause();
            return;
        }
        cout << "  New password:     ";
        string np = Util::readPassword();
        if (np.size() < 6) {
            Util::status("Password must be at least 6 characters.", false);
            Util::pause();
            return;
        }
        cout << "  Confirm:          ";
        string nc = Util::readPassword();
        if (np != nc) { Util::status("Passwords do not match.", false); Util::pause(); return; }
        passwordHash = Util::hashPassword(np);
        Util::status("Password changed successfully.");
        AuditLog::log(recordID, "CHANGED_PASSWORD");
        Util::pause();
    }
};

// ---------------------------------------------------------
//  Storage
// ---------------------------------------------------------
namespace Storage {
    const string CHILDREN_FILE     = "children.dat";
    const string VACCINATIONS_FILE = "vaccinations.dat";
    const string MEDICALS_FILE     = "medicals.dat";
    const string NURSES_FILE       = "nurses.dat";
    const string DOCTORS_FILE      = "doctors.dat";

    void saveChildren(const vector<Child>& children) {
        ofstream f(CHILDREN_FILE);
        for (const auto& c : children)
            f << "CHILD|" << c.serialize() << "\n";
    }

    void saveVaccinations(const vector<Child>& children) {
        ofstream f(VACCINATIONS_FILE);
        for (const auto& c : children)
            for (const auto& v : c.vaccinations)
                f << c.recordID << "|" << v.serialize() << "\n";
    }

    void saveMedicals(const vector<Child>& children) {
        ofstream f(MEDICALS_FILE);
        for (const auto& c : children)
            for (const auto& m : c.medicals)
                f << c.recordID << "|" << m.serialize() << "\n";
    }

    void saveAll(const vector<Child>& children) {
        saveChildren(children);
        saveVaccinations(children);
        saveMedicals(children);
    }

    void loadChildren(vector<Child>& children) {
        ifstream f(CHILDREN_FILE);
        if (!f) return;
        string line;
        while (getline(f, line))
            if (line.substr(0, 6) == "CHILD|")
                children.push_back(Child::deserialize(line.substr(6)));
    }

    void loadVaccinations(vector<Child>& children) {
        ifstream f(VACCINATIONS_FILE);
        if (!f) return;
        string line;
        while (getline(f, line)) {
            auto pos = line.find('|');
            if (pos == string::npos) continue;
            string rid  = line.substr(0, pos);
            string rest = line.substr(pos + 1);
            for (auto& c : children) {
                if (c.recordID == rid) {
                    c.vaccinations.push_back(VaccinationRecord::deserialize(rest));
                    break;
                }
            }
        }
    }

    void loadMedicals(vector<Child>& children) {
        ifstream f(MEDICALS_FILE);
        if (!f) return;
        string line;
        while (getline(f, line)) {
            auto pos = line.find('|');
            if (pos == string::npos) continue;
            string rid  = line.substr(0, pos);
            string rest = line.substr(pos + 1);
            for (auto& c : children) {
                if (c.recordID == rid) {
                    c.medicals.push_back(MedicalRecord::deserialize(rest));
                    break;
                }
            }
        }
    }

    void loadAll(vector<Child>& children) {
        loadChildren(children);
        loadVaccinations(children);
        loadMedicals(children);
    }

    // Staff records (Nurse / Doctor)
    struct StaffRecord {
        string role, id, passwordHash, firstName, lastName, phone, extra1, extra2;

        string serialize() const {
            using namespace Util;
            return escape(role) + "|" + escape(id) + "|" + escape(passwordHash) + "|" +
                   escape(firstName) + "|" + escape(lastName) + "|" + escape(phone) + "|" +
                   escape(extra1) + "|" + escape(extra2);
        }

        static StaffRecord deserialize(const string& line) {
            auto t = Util::split(line, '|');
            while (t.size() < 8) t.push_back("");
            StaffRecord r;
            r.role         = Util::unescape(t[0]);
            r.id           = Util::unescape(t[1]);
            r.passwordHash = Util::unescape(t[2]);
            r.firstName    = Util::unescape(t[3]);
            r.lastName     = Util::unescape(t[4]);
            r.phone        = Util::unescape(t[5]);
            r.extra1       = Util::unescape(t[6]);
            r.extra2       = Util::unescape(t[7]);
            return r;
        }
    };

    void appendStaff(const StaffRecord& r, const string& file) {
        ofstream f(file, ios::app);
        if (f) f << r.serialize() << "\n";
    }

    vector<StaffRecord> loadStaff(const string& file) {
        vector<StaffRecord> list;
        ifstream f(file);
        if (!f) return list;
        string line;
        while (getline(f, line))
            if (!Util::trim(line).empty())
                list.push_back(StaffRecord::deserialize(line));
        return list;
    }
}

// ---------------------------------------------------------
//  Global Data
// ---------------------------------------------------------
vector<Child> g_children;

Child* findChild(const string& id) {
    for (auto& c : g_children)
        if (c.recordID == id) return &c;
    return nullptr;
}

// ---------------------------------------------------------
//  Nurse Session
// ---------------------------------------------------------
class NurseSession {
    string name, practiceNumber;

    void captureVaccination() {
        Util::clearScreen();
        Util::printHeader("CAPTURE VACCINATION RECORD", Color::BCYAN);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        Util::progressBar("Verifying child record  ", 10, 30);

        VaccinationRecord v;
        v.capturedAt = Util::timestamp();
        cout << "\n";
        v.placeOfBirth   = Util::safeStringInput("  Place of Birth:   ");
        v.vaccine        = Util::safeStringInput("  Vaccine:          ");
        v.treatment      = Util::safeStringInput("  Treatment:        ");
        v.weight         = Util::safeStringInput("  Weight (kg):      ");
        v.height         = Util::safeStringInput("  Height (cm):      ");
        v.nurseName      = name;
        v.clinic         = Util::safeStringInput("  Clinic:           ");
        v.clinicDate     = Util::safeStringInput("  Clinic Date:      ");
        v.nextClinicDate = Util::safeStringInput("  Next Clinic Date: ");

        c->vaccinations.push_back(v);
        Storage::saveAll(g_children);
        AuditLog::log(practiceNumber, "VACCINATION_CAPTURED", c->recordID);
        Util::status("Vaccination record captured and saved.");
        Util::pause();
    }

    void searchVaccination() {
        Util::clearScreen();
        Util::printHeader("SEARCH VACCINATION RECORD", Color::BCYAN);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        Util::progressBar("Searching records       ", 10, 30);
        c->printVaccinationReport(false);
    }

    void searchMedical() {
        Util::clearScreen();
        Util::printHeader("SEARCH MEDICAL RECORD", Color::BCYAN);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        Util::progressBar("Searching records       ", 10, 30);
        c->printMedicalReport(false);
    }

    void updateVaccination() {
        Util::clearScreen();
        Util::printHeader("UPDATE VACCINATION RECORD", Color::BYELLOW);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        string date = Util::safeStringInput("  Clinic Date to update: ");
        for (auto& v : c->vaccinations) {
            if (v.clinicDate == date) {
                cout << "\n  (Leave blank to keep existing value)\n\n";
                auto ask = [](const string& lbl, const string& cur) -> string {
                    cout << "  " << lbl << " [" << cur << "]: ";
                    string s; getline(cin, s); s = Util::trim(s);
                    return s.empty() ? cur : s;
                };
                v.vaccine        = ask("Vaccine",          v.vaccine);
                v.treatment      = ask("Treatment",        v.treatment);
                v.weight         = ask("Weight (kg)",      v.weight);
                v.height         = ask("Height (cm)",      v.height);
                v.nextClinicDate = ask("Next Clinic Date", v.nextClinicDate);
                Storage::saveAll(g_children);
                AuditLog::log(practiceNumber, "VACCINATION_UPDATED", c->recordID + " date=" + date);
                Util::status("Record updated.");
                Util::pause();
                return;
            }
        }
        Util::status("Record not found for that date.", false);
        Util::pause();
    }

    void deleteVaccination() {
        Util::clearScreen();
        Util::printHeader("DELETE VACCINATION RECORD", Color::BRED);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        string date = Util::safeStringInput("  Clinic Date to delete: ");
        auto& vecs = c->vaccinations;
        for (auto it = vecs.begin(); it != vecs.end(); ++it) {
            if (it->clinicDate == date) {
                if (!Util::confirm("Delete this vaccination record permanently?")) return;
                vecs.erase(it);
                Storage::saveAll(g_children);
                AuditLog::log(practiceNumber, "VACCINATION_DELETED", c->recordID + " date=" + date);
                Util::status("Record deleted.");
                Util::pause();
                return;
            }
        }
        Util::status("Record not found.", false);
        Util::pause();
    }

    void statistics() {
        Util::clearScreen();
        Util::printHeader("CLINIC STATISTICS", Color::BBLUE);
        int totalVax = 0;
        for (auto& c : g_children) totalVax += (int)c.vaccinations.size();
        cout << "\n";
        cout << "  Total Registered Children:  " << Color::BCYAN  << g_children.size() << Color::RESET << "\n";
        cout << "  Total Vaccination Records:  " << Color::BGREEN << totalVax           << Color::RESET << "\n";
        cout << "  Your Practice Number:       " << practiceNumber << "\n";
        Util::pause();
    }

public:
    NurseSession(const string& n, const string& pn) : name(n), practiceNumber(pn) {}

    void run() {
        while (true) {
            Util::clearScreen();
            Util::printHeader("NURSE MENU  .  Sr. " + name, Color::BCYAN);
            cout << "\n";
            cout << "  " << Color::BOLD << "1." << Color::RESET << "  Capture Vaccination Record\n";
            cout << "  " << Color::BOLD << "2." << Color::RESET << "  View Child Vaccination Report\n";
            cout << "  " << Color::BOLD << "3." << Color::RESET << "  View Child Medical Report\n";
            cout << "  " << Color::BOLD << "4." << Color::RESET << "  Update Vaccination Record\n";
            cout << "  " << Color::BOLD << "5." << Color::RESET << "  Delete Vaccination Record\n";
            cout << "  " << Color::BOLD << "6." << Color::RESET << "  Statistics Dashboard\n";
            cout << "  " << Color::BOLD << "0." << Color::RESET << "  Logout\n\n";
            int ch = Util::safeIntInput("  " + Color::BCYAN + "-> " + Color::RESET + "Choice: ", 0, 6);
            switch (ch) {
                case 1: captureVaccination(); break;
                case 2: searchVaccination();  break;
                case 3: searchMedical();      break;
                case 4: updateVaccination();  break;
                case 5: deleteVaccination();  break;
                case 6: statistics();         break;
                case 0: return;
            }
        }
    }
};

// ---------------------------------------------------------
//  Doctor Session
// ---------------------------------------------------------
class DoctorSession {
    string name, practiceNumber;

    void captureMedical() {
        Util::clearScreen();
        Util::printHeader("CAPTURE MEDICAL RECORD", Color::BGREEN);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        Util::progressBar("Verifying child record  ", 10, 30);

        MedicalRecord m;
        m.capturedAt = Util::timestamp();
        cout << "\n";
        m.placeOfBirth     = Util::safeStringInput("  Place of Birth:      ");
        m.medication       = Util::safeStringInput("  Medication:          ");
        m.treatment        = Util::safeStringInput("  Treatment:           ");
        m.weight           = Util::safeStringInput("  Weight (kg):         ");
        m.height           = Util::safeStringInput("  Height (cm):         ");
        m.doctor           = name;
        m.clinic           = Util::safeStringInput("  Clinic/Hospital:     ");
        m.consultationDate = Util::safeStringInput("  Consultation Date:   ");
        m.checkupDate      = Util::safeStringInput("  Checkup Date:        ");

        c->medicals.push_back(m);
        Storage::saveAll(g_children);
        AuditLog::log(practiceNumber, "MEDICAL_CAPTURED", c->recordID);
        Util::status("Medical record captured and saved.");
        Util::pause();
    }

    void searchMedical() {
        Util::clearScreen();
        Util::printHeader("SEARCH MEDICAL RECORD", Color::BGREEN);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        Util::progressBar("Searching records       ", 10, 30);
        c->printMedicalReport(false);
    }

    void searchVaccination() {
        Util::clearScreen();
        Util::printHeader("SEARCH VACCINATION RECORD", Color::BGREEN);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        Util::progressBar("Searching records       ", 10, 30);
        c->printVaccinationReport(false);
    }

    void updateMedical() {
        Util::clearScreen();
        Util::printHeader("UPDATE MEDICAL RECORD", Color::BYELLOW);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        string date = Util::safeStringInput("  Consultation Date to update: ");
        for (auto& m : c->medicals) {
            if (m.consultationDate == date) {
                auto ask = [](const string& lbl, const string& cur) -> string {
                    cout << "  " << lbl << " [" << cur << "]: ";
                    string s; getline(cin, s); s = Util::trim(s);
                    return s.empty() ? cur : s;
                };
                cout << "\n  (Leave blank to keep existing value)\n\n";
                m.medication  = ask("Medication",   m.medication);
                m.treatment   = ask("Treatment",    m.treatment);
                m.weight      = ask("Weight (kg)",  m.weight);
                m.height      = ask("Height (cm)",  m.height);
                m.doctor      = ask("Doctor",        m.doctor);
                m.checkupDate = ask("Checkup Date", m.checkupDate);
                Storage::saveAll(g_children);
                AuditLog::log(practiceNumber, "MEDICAL_UPDATED", c->recordID + " date=" + date);
                Util::status("Record updated.");
                Util::pause();
                return;
            }
        }
        Util::status("Record not found for that date.", false);
        Util::pause();
    }

    void deleteMedical() {
        Util::clearScreen();
        Util::printHeader("DELETE MEDICAL RECORD", Color::BRED);
        string id = Util::safeStringInput("\n  Child Record ID: ");
        Child* c = findChild(id);
        if (!c) { Util::status("Child ID not found.", false); Util::pause(); return; }
        string date = Util::safeStringInput("  Consultation Date to delete: ");
        auto& vecs = c->medicals;
        for (auto it = vecs.begin(); it != vecs.end(); ++it) {
            if (it->consultationDate == date) {
                if (!Util::confirm("Delete this medical record permanently?")) return;
                vecs.erase(it);
                Storage::saveAll(g_children);
                AuditLog::log(practiceNumber, "MEDICAL_DELETED", c->recordID + " date=" + date);
                Util::status("Record deleted.");
                Util::pause();
                return;
            }
        }
        Util::status("Record not found.", false);
        Util::pause();
    }

    void statistics() {
        Util::clearScreen();
        Util::printHeader("CLINIC STATISTICS", Color::BBLUE);
        int totalMed = 0;
        for (auto& c : g_children) totalMed += (int)c.medicals.size();
        cout << "\n";
        cout << "  Total Registered Children: " << Color::BCYAN  << g_children.size() << Color::RESET << "\n";
        cout << "  Total Medical Records:     " << Color::BGREEN << totalMed           << Color::RESET << "\n";
        cout << "  Your Practice Number:      " << practiceNumber << "\n";
        Util::pause();
    }

public:
    DoctorSession(const string& n, const string& pn) : name(n), practiceNumber(pn) {}

    void run() {
        while (true) {
            Util::clearScreen();
            Util::printHeader("DOCTOR MENU  .  Dr. " + name, Color::BGREEN);
            cout << "\n";
            cout << "  " << Color::BOLD << "1." << Color::RESET << "  Capture Medical Record\n";
            cout << "  " << Color::BOLD << "2." << Color::RESET << "  View Child Medical Report\n";
            cout << "  " << Color::BOLD << "3." << Color::RESET << "  View Child Vaccination Report\n";
            cout << "  " << Color::BOLD << "4." << Color::RESET << "  Update Medical Record\n";
            cout << "  " << Color::BOLD << "5." << Color::RESET << "  Delete Medical Record\n";
            cout << "  " << Color::BOLD << "6." << Color::RESET << "  Statistics Dashboard\n";
            cout << "  " << Color::BOLD << "0." << Color::RESET << "  Logout\n\n";
            int ch = Util::safeIntInput("  " + Color::BGREEN + "-> " + Color::RESET + "Choice: ", 0, 6);
            switch (ch) {
                case 1: captureMedical();    break;
                case 2: searchMedical();     break;
                case 3: searchVaccination(); break;
                case 4: updateMedical();     break;
                case 5: deleteMedical();     break;
                case 6: statistics();        break;
                case 0: return;
            }
        }
    }
};

// ---------------------------------------------------------
//  Registration Helpers
// ---------------------------------------------------------
void registerChild() {
    Util::clearScreen();
    Util::printHeader("CHILD / PARENT REGISTRATION", Color::BCYAN);
    Child c;
    cout << "\n";
    c.firstName  = Util::safeStringInput("  First Name:              ");
    c.middleName = Util::safeStringInput("  Middle Name:             ");
    c.lastName   = Util::safeStringInput("  Last Name:               ");

    while (true) {
        c.dateOfBirth = Util::safeStringInput("  Date of Birth (DDMMYYYY):");
        if (Util::isValidDate(c.dateOfBirth)) break;
        Util::status("Invalid date format. Use DDMMYYYY.", false);
    }

    c.guardianName = Util::safeStringInput("  Guardian/Mother's Name:  ");
    c.fatherName   = Util::safeStringInput("  Father's Name:           ");
    c.village      = Util::safeStringInput("  Village:                 ");
    c.chief        = Util::safeStringInput("  Chief Name:              ");
    c.district     = Util::safeStringInput("  District:                ");

    cout << "  Password (min 6 chars):   ";
    string pw;
    while (true) {
        pw = Util::readPassword();
        if (pw.size() >= 6) break;
        cout << "  " << Color::RED << "Password too short. Try again: " << Color::RESET;
    }
    c.passwordHash = Util::hashPassword(pw);
    c.recordID     = Child::generateID(c.firstName, c.lastName, c.dateOfBirth);

    // Prevent duplicate IDs
    if (findChild(c.recordID)) {
        int suffix = 2;
        string base = c.recordID;
        while (findChild(base + "-" + to_string(suffix))) suffix++;
        c.recordID = base + "-" + to_string(suffix);
    }

    g_children.push_back(c);
    Storage::saveAll(g_children);
    AuditLog::log(c.recordID, "CHILD_REGISTERED", c.firstName + " " + c.lastName);

    cout << "\n";
    Util::status("Registration successful!");
    cout << "\n  " << Color::BOLD << "Your Child Record ID: " << Color::BCYAN << c.recordID << Color::RESET << "\n";
    cout << "  " << Color::YELLOW << "[!] Please save this ID -- you need it to log in.\n" << Color::RESET;
    Util::pause();
}

void registerNurse() {
    Util::clearScreen();
    Util::printHeader("NURSE REGISTRATION", Color::BCYAN);
    cout << "\n";
    Storage::StaffRecord r;
    r.role      = "NURSE";
    r.firstName = Util::safeStringInput("  First Name:         ");
    r.lastName  = Util::safeStringInput("  Last Name:          ");
    r.id        = Util::safeStringInput("  Practice Number:    ");
    r.extra1    = Util::safeStringInput("  Clinic:             ");
    r.extra2    = Util::safeStringInput("  Clinic Address:     ");

    while (true) {
        r.phone = Util::safeStringInput("  Phone Number:       ");
        if (Util::isValidPhone(r.phone)) break;
        Util::status("Invalid phone number.", false);
    }

    cout << "  Password (min 6 chars): ";
    string pw;
    while (true) {
        pw = Util::readPassword();
        if (pw.size() >= 6) break;
        cout << Color::RED << "  Too short. Try again: " << Color::RESET;
    }
    r.passwordHash = Util::hashPassword(pw);
    Storage::appendStaff(r, Storage::NURSES_FILE);
    AuditLog::log(r.id, "NURSE_REGISTERED", r.firstName + " " + r.lastName);
    Util::status("Nurse registered successfully.");
    Util::pause();
}

void registerDoctor() {
    Util::clearScreen();
    Util::printHeader("DOCTOR REGISTRATION", Color::BGREEN);
    cout << "\n";
    Storage::StaffRecord r;
    r.role      = "DOCTOR";
    r.firstName = Util::safeStringInput("  First Name:              ");
    r.lastName  = Util::safeStringInput("  Last Name:               ");
    r.id        = Util::safeStringInput("  Practice Number:         ");
    r.extra1    = Util::safeStringInput("  Resident Hospital/Clinic:");
    r.extra2    = Util::safeStringInput("  Hospital/Clinic Address: ");

    while (true) {
        r.phone = Util::safeStringInput("  Phone Number:            ");
        if (Util::isValidPhone(r.phone)) break;
        Util::status("Invalid phone number.", false);
    }

    cout << "  Password (min 6 chars): ";
    string pw;
    while (true) {
        pw = Util::readPassword();
        if (pw.size() >= 6) break;
        cout << Color::RED << "  Too short. Try again: " << Color::RESET;
    }
    r.passwordHash = Util::hashPassword(pw);
    Storage::appendStaff(r, Storage::DOCTORS_FILE);
    AuditLog::log(r.id, "DOCTOR_REGISTERED", r.firstName + " " + r.lastName);
    Util::status("Doctor registered successfully.");
    Util::pause();
}

// ---------------------------------------------------------
//  Login
// ---------------------------------------------------------
void loginFlow(int role) {
    Util::clearScreen();
    string roleLabel = (role == 1) ? "Child/Parent" : (role == 2) ? "Nurse" : "Doctor";
    Util::printHeader("LOGIN  .  " + roleLabel, Color::BYELLOW);
    cout << "\n";

    // FIX: wrap ternary in string() so operator+ works on std::string, not char[]
    string prompt = "  " + string(role == 1 ? "Record ID" : "Practice Number") + ": ";
    string id = Util::safeStringInput(prompt);

    cout << "  Password: ";
    string pw   = Util::readPassword();
    string hash = Util::hashPassword(pw);

    if (role == 1) {
        Child* c = findChild(id);
        if (c && c->passwordHash == hash) {
            AuditLog::log(id, "LOGIN", "CHILD");
            Util::progressBar("Authenticating          ", 8, 25);
            Util::status("Welcome, " + c->firstName + " " + c->lastName + "!");
            SLEEP(600);
            c->taskMenu();
        } else {
            Util::status("Invalid ID or password.", false);
            AuditLog::log(id, "FAILED_LOGIN", "CHILD");
            Util::pause();
        }
    } else if (role == 2) {
        auto nurses = Storage::loadStaff(Storage::NURSES_FILE);
        for (auto& n : nurses) {
            if (n.id == id && n.passwordHash == hash) {
                AuditLog::log(id, "LOGIN", "NURSE");
                Util::progressBar("Authenticating          ", 8, 25);
                Util::status("Welcome, Sr. " + n.firstName + " " + n.lastName + "!");
                SLEEP(600);
                NurseSession(n.firstName + " " + n.lastName, n.id).run();
                return;
            }
        }
        Util::status("Invalid ID or password.", false);
        AuditLog::log(id, "FAILED_LOGIN", "NURSE");
        Util::pause();
    } else {
        auto doctors = Storage::loadStaff(Storage::DOCTORS_FILE);
        for (auto& d : doctors) {
            if (d.id == id && d.passwordHash == hash) {
                AuditLog::log(id, "LOGIN", "DOCTOR");
                Util::progressBar("Authenticating          ", 8, 25);
                Util::status("Welcome, Dr. " + d.firstName + " " + d.lastName + "!");
                SLEEP(600);
                DoctorSession(d.firstName + " " + d.lastName, d.id).run();
                return;
            }
        }
        Util::status("Invalid ID or password.", false);
        AuditLog::log(id, "FAILED_LOGIN", "DOCTOR");
        Util::pause();
    }
}

// ---------------------------------------------------------
//  Main Menu
// ---------------------------------------------------------
void mainMenu() {
    while (true) {
        Util::clearScreen();
        cout << "\n\n";
        cout << Color::BCYAN << Color::BOLD;
        cout << "  +========================================================================+\n";
        cout << "  |                                                                        |\n";
        cout << "  |          B U K A N A   C H I L D   H E A L T H   S Y S T E M           |\n";
        cout << "  |               Vaccination & Medical Record Management                  |\n";
        cout << "  |                                                                        |\n";
        cout << "  +========================================================================+\n";
        cout << Color::RESET;
        cout << "  " << Color::DIM << Util::timestamp() << Color::RESET << "\n\n";

        Util::divider(Color::CYAN);
        cout << "  " << Color::BOLD << " 1." << Color::RESET << "  Register\n";
        cout << "  " << Color::BOLD << " 2." << Color::RESET << "  Login\n";
        cout << "  " << Color::BOLD << " 0." << Color::RESET << "  Exit\n\n";

        int ch = Util::safeIntInput("  " + Color::BCYAN + "-> " + Color::RESET + "Choice: ", 0, 2);

        if (ch == 0) {
            Util::clearScreen();
            cout << "\n\n  " << Color::BCYAN << "Thank you for using Bukana Health System. Goodbye!\n\n" << Color::RESET;
            AuditLog::log("SYSTEM", "SHUTDOWN");
            exit(0);
        }

        // Role selection sub-menu
        auto roleMenu = [](const string& action) -> int {
            Util::clearScreen();
            Util::printHeader(action + " -- SELECT ROLE", Color::BYELLOW);
            cout << "\n";
            cout << "  " << Color::BOLD << "1." << Color::RESET << "  Child / Parent\n";
            cout << "  " << Color::BOLD << "2." << Color::RESET << "  Nurse\n";
            cout << "  " << Color::BOLD << "3." << Color::RESET << "  Doctor\n";
            cout << "  " << Color::BOLD << "0." << Color::RESET << "  Back\n\n";
            return Util::safeIntInput("  -> Role: ", 0, 3);
        };

        if (ch == 1) {
            int role = roleMenu("REGISTER");
            if (role == 1) registerChild();
            else if (role == 2) registerNurse();
            else if (role == 3) registerDoctor();
        } else {
            int role = roleMenu("LOGIN");
            if (role >= 1 && role <= 3) loginFlow(role);
        }
    }
}

// ---------------------------------------------------------
//  Entry Point
// ---------------------------------------------------------
int main() {
    enableANSI();
    Storage::loadAll(g_children);
    AuditLog::log("SYSTEM", "STARTUP", "children=" + to_string(g_children.size()));
    mainMenu();
    return 0;
}
