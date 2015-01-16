#pragma once

#include <boost\noncopyable.hpp>

#include "common.h"

namespace common
{
	class Meas: public boost::noncopyable
	{
	public:
		typedef std::shared_ptr<Meas> PMeas;
	public:
		Meas();
		~Meas();

		static PMeas empty();
	public:
		Id   id;                     // �������������
		Time time;                   // ����� ���������
		Flag source;                 // ������� ����� ��� ����������� ��������
		Flag flag;                   // ����� �� ������ �������
		size_t size;                 // ������ ������
		std::shared_ptr<void*> data; // ���� ������
	};
}