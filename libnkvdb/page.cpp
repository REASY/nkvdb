#include "page.h"
#include "exception.h"
#include "search.h"
#include "readers.h"

#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <boost/filesystem.hpp>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

uint64_t nkvdb::PageReader::ReadSize=nkvdb::PageReader::defaultReadSize;
namespace bi=boost::interprocess;

using namespace nkvdb;

const size_t oneMb = sizeof(char) * 1024 * 1024;

bool nkvdb::HeaderIntervalCheck(Time from, Time to, Page::Header hdr) {
	if (utils::inInterval(from, to, hdr.minTime) || utils::inInterval(from, to, hdr.maxTime)) {
		return true;
	} else {
		return false;
	}
}

bool nkvdb::HeaderIdIntervalCheck(Id from, Id to, Page::Header hdr) {
	if (hdr.minId >= from || hdr.maxId <= to) {
		return true;
	} else {
		return false;
	}
}

Page::Page(std::string fname)
    : m_filename(new std::string(fname)),
      m_file(nullptr),
      m_region(nullptr)
{
	
	this->m_index.setFileName(this->index_fileName());
}

Page::~Page() {
    this->close();
    delete m_filename;
}

void Page::close() {
    if ((this->m_file!=nullptr) && (m_region!=nullptr)) {
        //logger("write_window.size="<<m_writewindow.size());
        this->flushWriteWindow();
        this->m_header->isOpen = false;
        this->m_header->ReadersCount = 0;
		m_region->flush(0, 0, false);
        delete m_region;
        delete m_file;
        m_region=nullptr;
        m_file=nullptr;

    }
}

void Page::flushWriteWindow(){
    this->m_header->WriteWindowSize=m_writewindow.size();
    if(this->m_header->WriteWindowSize!=0){
        std::ofstream ofs;
        ofs.open(this->writewindow_fileName(),std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
        assert(ofs.is_open());

        for(auto kv:m_writewindow){
            ofs.write((char*)&kv,sizeof(Meas));
        }

        ofs.close();
    }
}

void Page::loadWriteWindow(){

	std::ifstream ifs(this->writewindow_fileName(), std::ifstream::binary | std::ifstream::in);
    if(!ifs.is_open()){
		/// this is not error. write window file is no exists, when openned empty page without data.
        return;
    }

    while(true){
        Meas value;
        if(!ifs.read((char*)&value,sizeof(value))){
			this->updateWriteWindow(value);
            break;
        }else{
			this->updateWriteWindow(value);
        }
    }

}

size_t Page::size() const { return m_region->get_size(); }

std::string Page::fileName() const { return std::string(*m_filename); }

std::string Page::index_fileName() const {
  return std::string(*m_filename) + "i";
}

std::string Page::writewindow_fileName() const {
	return std::string(*m_filename) + "w";
}

Time Page::minTime() const { 
	return m_header->minTime; 
}

Time Page::maxTime() const { 
	return m_header->maxTime; 
}

Page::Page_ptr Page::Open(std::string filename, bool readOnly) {
    if(!readOnly){
        nkvdb::Page::Header hdr = Page::ReadHeader(filename);
        if (hdr.isOpen) {
			std::stringstream ss;
			ss << "page is already openned. filename=" << filename << " readOnly=" << readOnly;
			throw MAKE_EXCEPTION(ss.str());
        }
    }
    Page_ptr result(new Page(filename));

    try {
           result->m_file=new bi::file_mapping(filename.c_str(),bi::read_write);
           result->m_region=new bi::mapped_region(*result->m_file, bi::read_write);
    } catch (std::runtime_error &ex) {
        std::string what = ex.what();
        throw MAKE_EXCEPTION(ex.what());
    }

    char *data = static_cast<char*>(result->m_region->get_address());
    result->m_header = (Page::Header *)data;
    result->m_data_begin = (Meas *)(data + sizeof(Page::Header));

    result->m_header->isOpen = true;
    if(readOnly){
        result->m_header->ReadersCount+=1;
    }
	result->m_region->flush(0, sizeof(result->m_header), false);

    result->loadWriteWindow();

    return result;
}

Page::Page_ptr Page::Create(std::string filename, uint64_t fsize) {
  Page_ptr result(new Page(filename));

  try {
      {
          bi::file_mapping::remove(filename.c_str());
          std::filebuf fbuf;
          fbuf.open(filename,
                    std::ios_base::in | std::ios_base::out|std::ios_base::trunc | std::ios_base::binary);
                   //Set the size
          fbuf.pubseekoff(fsize-1, std::ios_base::beg);
          fbuf.sputc(0);
      }
      result->m_file=new bi::file_mapping(filename.c_str(),bi::read_write);
      result->m_region=new bi::mapped_region(*result->m_file, bi::read_write);
  } catch (std::runtime_error &ex) {
	  std::string what = ex.what();
	  throw MAKE_EXCEPTION(ex.what());
  }

  char *data = static_cast<char*>(result->m_region->get_address());

  result->initHeader(data);
  result->m_data_begin = (Meas *)(data + sizeof(Page::Header));
  result->m_header->isOpen = true;
  result->m_region->flush(0, sizeof(result->m_header), false);
  return result;
}

Page::Header Page::ReadHeader(std::string filename) {
  std::ifstream istream;
  istream.open(filename, std::fstream::in|std::fstream::binary);
  if (!istream.is_open()) {
	  std::stringstream ss;
	  ss << "can't open file. filename=" << filename;
	  throw MAKE_EXCEPTION(ss.str());
  }
  Header result;
  memset(&result, 0, sizeof(Header));
  istream.read((char *)&result, sizeof(Page::Header));
  istream.close();
  return result;
}

void Page::initHeader(char *data) {
  m_header = (Page::Header *)data;
  memset(m_header, 0, sizeof(Page::Header));
  m_header->version = page_version;
  m_header->size = this->size();
  m_header->minMaxInit = false;
}

void Page::updateMinMax(const Meas& value) {
  if (m_header->minMaxInit) {
    m_header->minTime = std::min(value.time, m_header->minTime);
    m_header->maxTime = std::max(value.time, m_header->maxTime);

    m_header->minId = std::min(value.id, m_header->minId);
    m_header->maxId = std::max(value.id, m_header->maxId);
	
  } else {
    m_header->minMaxInit = true;
    m_header->minTime = value.time;
    m_header->maxTime = value.time;

    m_header->minId = value.id;
    m_header->maxId = value.id;
  }
}

void Page::updateWriteWindow(const Meas&m) {
	if (m_writewindow.size() <= m.id) {
		m_writewindow.resize(m.id+1);
		m_writewindow[m.id] = m;
	} else {
		auto old_value = m_writewindow[m.id];
		if (old_value.time<m.time) {
			m_writewindow[m.id] = m;
		}
	}
}

bool Page::append(const Meas& value) {
    assert(m_header->ReadersCount==0);
    if (this->isFull()) {
        return false;
    }

    updateMinMax(value);

	updateWriteWindow(value);
    m_header->WriteWindowSize=m_writewindow.size();


    memcpy(&m_data_begin[m_header->write_pos], &value, sizeof(Meas));

    Index::IndexRecord rec;
    rec.minTime = value.time;
    rec.maxTime = value.time;
    rec.minId = value.id;
    rec.maxId = value.id;
    rec.count = 1;
    rec.pos = m_header->write_pos;

    this->m_index.writeIndexRec(rec);

    m_header->write_pos++;
    return true;
}

size_t Page::append(const Meas::PMeas begin, const size_t size) {
    assert(m_header->ReadersCount==0);
    size_t cap = this->capacity();
    size_t to_write = 0;
    if (cap == 0) {
        return 0;
    }
    if (cap > size) {
        to_write = size;
    } else if (cap == size) {
        to_write = size;
    } else if (cap < size) {
        to_write = cap;
    }
    memcpy(m_data_begin + m_header->write_pos, begin, to_write * sizeof(Meas));

    for(auto it=begin;it!=begin+to_write;it++){
		updateWriteWindow(*it);
    }
    m_header->WriteWindowSize=m_writewindow.size();

    updateMinMax(begin[0]);
    updateMinMax(begin[to_write-1]);
	//m_region->flush(0, this->size(), false);

    Index::IndexRecord rec;
    rec.minTime = begin[0].time;
    rec.maxTime = begin[to_write - 1].time;
    rec.minId = begin[0].id;
    rec.maxId = begin[to_write - 1].id;
    rec.count = to_write;
    rec.pos = m_header->write_pos;

    this->m_index.writeIndexRec(rec);
	
    m_header->write_pos += to_write;
    return to_write;
}



bool Page::read(Meas::PMeas result, uint64_t position) {
    if(this->m_header->write_pos==0){
        return false;
    }
    if (result == nullptr)
        return false;
    {
        if (m_header->write_pos <= position) {
            return false;
        }
    }

    Meas *m = &m_data_begin[position];
    result->readFrom(m);
    return true;
}

void Page::readComplete(){
    std::lock_guard<std::mutex> _lock(m_lock);

    if(m_header->ReadersCount!=0){

        this->m_header->ReadersCount--;

        if(m_header->ReadersCount==0){
            this->close();
        }
    }
}

WriteWindow Page::getWriteWindow(){
    return WriteWindow(m_writewindow.begin(),m_writewindow.end());
}

void Page::setWriteWindow(const WriteWindow&other){
    m_writewindow=WriteWindow{other.begin(),other.end()};
}

PageReader_ptr  Page::readAll() {
    if(this->m_header->write_pos==0){
        return nullptr;
    }
    auto ppage=this->shared_from_this();
    auto preader=new PageReaderInterval(ppage);
    preader->addReadPos(0,m_header->write_pos);

    auto result=PageReader_ptr(preader);
    return result;
}


PageReader_ptr Page::readFromToPos(const IdArray &ids, nkvdb::Flag source, nkvdb::Flag flag, Time from, Time to,size_t begin,size_t end){
    if(this->m_header->write_pos==0){
        return nullptr;
    }

    auto ppage=this->shared_from_this();
    auto preader=new PageReaderInterval(ppage);
    auto result=PageReader_ptr(preader);
    preader->addReadPos(begin,end);
    preader->ids=ids;
    preader->source=source;
    preader->flag=flag;
    preader->from=from;
    preader->to=to;
    return result;
}

PageReader_ptr Page::readInterval(Time from, Time to) {
    if(this->m_header->write_pos==0){
        return nullptr;
    }
    static IdArray emptyArray;
    return this->readInterval(emptyArray, 0, 0, from, to);
}

PageReader_ptr Page::readInterval(const IdArray &ids, nkvdb::Flag source, nkvdb::Flag flag, Time from, Time to) {
    // [from...minTime,maxTime...to]
    if(this->m_header->write_pos==0){
        return nullptr;
    }
	if (from > this->m_header->maxTime) {
		/// read from write window
		auto ppage = this->shared_from_this();
        auto preader = new PageReaderInterval(ppage);
		auto result = PageReader_ptr(preader);
        preader->ids = ids;
        preader->source = source;
        preader->flag = flag;
        preader->from = from;
        preader->to = to;
        preader->isWindowReader = true;
		return result;
	}
    if ((from <= m_header->minTime) && (to >= m_header->maxTime)) {
        if ((ids.size() == 0) && (source == 0) && (flag == 0)) {
            auto result=this->readAll();
            auto preader=dynamic_cast<PageReaderInterval*>(result.get());
            preader->from=from;
            preader->to=to;
            return result;
        } else {
            return this->readFromToPos(ids, source, flag, from, to, 0, m_header->write_pos);
        }
    }
	m_region->flush(0, this->size(), false);
    auto ppage=this->shared_from_this();
    auto preader=new PageReaderInterval(ppage);
    auto result=PageReader_ptr(preader);
    preader->ids = ids;
    preader->source = source;
    preader->flag = flag;
    preader->from = from;
    preader->to = to;

    auto irecords = m_index.findInIndex(ids, from, to);
    for (Index::IndexRecord &rec : irecords) {
        auto max_pos = rec.pos + rec.count;
        preader->addReadPos(rec.pos,max_pos);
    }
  return result;
}

PageReader_ptr Page::readInTimePoint(Time time_point) {
	static IdArray emptyArray;
	return this->readInTimePoint(emptyArray, 0, 0, time_point);
}

PageReader_ptr Page::readInTimePoint(const IdArray &ids, nkvdb::Flag source, nkvdb::Flag flag, Time time_point) {
	if (this->m_header->write_pos == 0) {
		return nullptr;
	}
	auto ppage = this->shared_from_this();
    auto preader = new PageReader_TimePoint(ppage);
	auto result = PageReader_ptr(preader);
    preader->ids = ids;
    preader->source = source;
    preader->flag = flag;
    preader->time_point = time_point;
	return result;
}

Meas::MeasList Page::backwardRead(const IdArray &ids, nkvdb::Flag source, nkvdb::Flag flag, Time time_point) {
	Meas::MeasList result;
	
	std::map<Id, Meas> readed_values{};
	for (uint64_t pos = this->getHeader().write_pos-1;; pos--) {
		Meas m;
		if (!this->read(&m, pos)) {
            std::stringstream ss;
            ss<<"Page::backwardRead read error pos="<<pos<<" start="<<this->getHeader().write_pos-1;
            throw MAKE_EXCEPTION(ss.str());
		}
		
		if (m.time < time_point) {
			auto find_res = readed_values.find(m.id);
			if (find_res == readed_values.end()) {
				bool flag_check = true;
				if (flag != 0) {
					if (m.flag != flag) {
						flag_check = false;
					}
				}
				bool source_check = true;
				if (source != 0) {
					if (m.source != source) {
						source_check = false;
					}
				}
				bool ids_check = true;
				if (ids.size() != 0) {
					if (std::find(ids.cbegin(), ids.cend(), m.id) != ids.end()) {
						ids_check = false;
					}
				}
				if (flag_check && source_check && ids_check) {
					readed_values.insert(std::make_pair(m.id, m));
				}
			} else {
				if (find_res->second.time < m.time) {
					readed_values[m.id] = m;
				}
			}
		}
		if (pos == 0) {
			break;
		}
	}
	for (auto kv : readed_values) {
		result.push_back(kv.second);
	}
	return result;
}

bool Page::isFull() const {
  return (sizeof(Page::Header) + sizeof(nkvdb::Meas) * m_header->write_pos) >= m_header->size;
}

size_t Page::capacity() const {
  size_t bytes_left = m_header->size -(sizeof(Page::Header) + sizeof(nkvdb::Meas) * m_header->write_pos);
  return bytes_left / sizeof(Meas);
}

Page::Header Page::getHeader() const { return *m_header; }

PageReader::PageReader(Page::Page_ptr page):
    ids(),
    source(0),
    flag(0),
    prev_ww()
{
    m_page=page;
}

PageReader::~PageReader(){
    if(m_page!=nullptr){
        m_page->readComplete();
        m_page=nullptr;
    }
}

bool PageReader::checkValueFlags(const Meas&m)const {
    if (flag != 0) {
        if (m.flag != flag) {
            return false;
        }
    }
    if (source != 0) {
        if (m.source != source) {
            return false;
        }
    }
    if (ids.size() != 0) {
        if (std::find(ids.cbegin(), ids.cend(), m.id) == ids.end()) {
            return false;
        }
    }
    return true;
}

void PageReader::readAll(Meas::MeasList*output) {
    while (!isEnd()) {
        this->readNext(output);
    }
}

void PageReader::timePointRead(Time tp,Meas::MeasList*output) {
    if (tp > this->m_page->getHeader().maxTime) {
        for (auto wwIt : this->m_page->getWriteWindow()) {
            if (wwIt.time == 0) {
                continue;
            }
            auto readedValue = wwIt;
            if (checkValueFlags(readedValue)) {
                output->push_back(readedValue);
            }
        }
    } else {
        auto sub_result = this->m_page->backwardRead(this->ids, source, flag, tp);

        // to ouput pushing values from prev_ww, that no exists in sub_result
        for (auto wwIt : prev_ww) {
            if (wwIt.time == 0) {
                continue;
            }
            bool exists = false;
            for (auto sub_value : sub_result) {
                if (sub_value.id == wwIt.id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                auto readedValue = wwIt;
                if (checkValueFlags(readedValue)) {
                    output->push_back(readedValue);
                }
            }
        }
        std::copy(sub_result.begin(), sub_result.end(), std::back_inserter(*output));
    }
}
