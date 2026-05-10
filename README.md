# Bukana-Child-Vaccination-Record-Management-System

![Project Status](https://img.shields.io/badge/status-active-brightgreen)
![Language](https://img.shields.io/badge/language-C%2B%2B-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)

## 🏥 Advanced C++ System for Managing Child Vaccination and Medical Records

A comprehensive, secure, and advanced C++ console application for managing child vaccination and medical records in Bukana health facilities.

## 🚀 Features

### Security & Authentication
- **Secure Password Hashing**: djb2 hash algorithm implementation
- **Password Masking**: Input passwords are hidden for security
- **Role-Based Access Control**: Separate interfaces for children/parents, nurses, and doctors
- **Audit Logging**: Complete tracking of all system actions with timestamps

### Data Management
- **Child Registration**: Comprehensive registration with unique ID generation
- **Vaccination Records**: Full CRUD operations for vaccination history
- **Medical Records**: Complete medical history management
- **Persistent Storage**: JSON-style flat file storage with proper serialization
- **Duplicate ID Prevention**: Automatic handling of duplicate record IDs

### User Interfaces
- **Child/Parent Portal**: View vaccination and medical reports
- **Nurse Dashboard**: Capture, search, update, and delete vaccination records
- **Doctor Dashboard**: Capture, search, update, and delete medical records
- **Statistics Dashboard**: Real-time statistics on registered children and records

### Advanced Features
- **Input Validation**: Comprehensive validation for all user inputs
- **Data Masking**: Privacy protection with masked display options
- **Search Functionality**: Search by name, ID, or date
- **Report Export**: Export vaccination and medical reports to text files
- **Cross-Platform Support**: ANSI color codes work on Windows 10+ and Linux
- **Progress Animations**: Professional UI with progress bars and typewriter effects
- **Graceful Error Handling**: Robust error management throughout the application

## 📋 System Requirements

- C++11 or later compiler (GCC 5.1+, Clang 3.3+, or MSVC 2015+)
- Windows 10+ (with ANSI support) or Linux/Unix system
- Terminal with ANSI color support

## 🔧 Quick Start

### On Linux/Mac:
```bash
git clone https://github.com/tahleho3968/Bukana-Child-Vaccination-Record-Management-System.git
cd Bukana-Child-Vaccination-Record-Management-System
g++ -std=c++11 -O2 main.cpp -o bukana_health
./bukana_health
```

### On Windows:
```bash
git clone https://github.com/tahleho3968/Bukana-Child-Vaccination-Record-Management-System.git
cd Bukana-Child-Vaccination-Record-Management-System
g++ -std=c++11 -O2 main.cpp -o bukana_health.exe
bukana_health.exe
```

## 📊 System Architecture

The system follows a modular architecture with clear separation of concerns:

- **Presentation Layer**: ANSI-based console interface with color-coded menus
- **Business Logic Layer**: Authentication, validation, and record management
- **Data Access Layer**: File I/O, serialization, and audit logging
- **Data Storage Layer**: Flat files with pipe-delimited record formats

### Data Storage Files
- `children.dat` - Child registration records
- `vaccinations.dat` - Vaccination history records
- `medicals.dat` - Medical history records
- `nurses.dat` - Nurse staff records
- `doctors.dat` - Doctor staff records
- `AuditLog.txt` - Complete system audit trail

## 👥 User Roles

| Role | Register Child | View Records | Capture Records | Update/Delete Records | View Statistics |
|------|:---:|:---:|:---:|:---:|:---:|
| Child/Parent | ✅ | Masked View | ❌ | ❌ | ❌ |
| Nurse | ❌ | Full View | Vaccination | Vaccination | ✅ |
| Doctor | ❌ | Full View | Medical | Medical | ✅ |

## 📝 Record ID Format

Child records use unique IDs: `XXX-XXX-DDMMYYYY`
- First 3 letters of first name (uppercase)
- First 3 letters of last name (uppercase)  
- Date of birth in DDMMYYYY format

## 📚 Documentation

📥 **[Download Full System Documentation (PDF)](https://tahleho3968.github.io/Bukana-Child-Vaccination-Record-Management-System/system-documentation.pdf)**

The comprehensive documentation covers:
- System architecture and design patterns
- Database design and data flow diagrams
- Security architecture and access control
- User interface design guidelines
- Technical specifications and requirements
- Installation guides for all platforms
- API reference documentation
- Testing strategies and quality metrics
- Maintenance and support procedures

## 🆕 What's New (v2.0)

- Complete input validation for all user inputs
- Persistent storage with proper serialization
- Comprehensive audit log for all record changes
- Enhanced search by name, ID, or date
- Statistics dashboard for monitoring
- Password masking at input
- Duplicate ID prevention
- Professional UI with progress animations
- Cross-platform compatibility
- Export functionality for reports

## 👨‍💻 Developer

**Tahleho**
- GitHub: [@tahleho3968](https://github.com/tahleho3968)

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---
*Bukana Child Vaccination & Medical Record Management System - Ensuring better health records for our children*
